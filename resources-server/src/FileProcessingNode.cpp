#include <absl/strings/escaping.h> /*base64*/
#include <config/ServerConfig.hpp>
#include <dispatcher/RequestHandlerDispatcher.hpp>
#include <handler/FileProcessingNode.hpp>
#include <spdlog/spdlog.h>

handler::FileProcessingNode::FileProcessingNode() : FileProcessingNode(0) {}

handler::FileProcessingNode::FileProcessingNode(const std::size_t id)
    : m_stop(false), processing_id(id) {

  /*start processing thread to process queue*/
  m_working = std::thread(&FileProcessingNode::processing, this);
}

handler::FileProcessingNode::~FileProcessingNode() { shutdown(); }

void handler::FileProcessingNode::setProcessingId(const std::size_t id) {
  processing_id = id;
}

const std::size_t handler::FileProcessingNode::getProcessingId() const {
  return processing_id;
}

void handler::FileProcessingNode::shutdown() {

  m_stop = true;
  m_cv.notify_all();

  if (m_working.joinable()) {
    m_working.join();
  }
}

void handler::FileProcessingNode::processing() {
  for (;;) {
    std::unique_lock<std::mutex> _lckg(m_mtx);
    m_cv.wait(_lckg, [this]() { return m_stop || !m_queue.empty(); });

    if (m_stop) {
      /*take care of the rest of the tasks, and shutdown synclogic*/
      while (!m_queue.empty()) {

        /*execute file handler*/
        execute(std::move(m_queue.front()));
        m_queue.pop();
      }
      return;
    }

    // auto &front = m_queue.front();
    execute(std::move(m_queue.front()));
    m_queue.pop();
  }
}

void handler::FileProcessingNode::execute(pair &&block) {
  if (block.second->direction == TransferDirection::Download) {
    download(tools::static_unique_ptr_cast<FileDownloadDescription>(
                 std::move(block.second)),
             block.first);
  } else if (block.second->direction == TransferDirection::Upload) {
    upload(tools::static_unique_ptr_cast<FileUploadDescription>(
               std::move(block.second)),
           block.first);
  } else {
    spdlog::error("[{}]: Invalid TransferDirection. It could only be Download "
                  "or Transfer",
                  ServerConfig::get_instance()->GrpcServerName);
  }
}

bool handler::FileProcessingNode::validFilename(std::string_view name) {
  return name.find("..") == std::string::npos &&
         name.find('/') == std::string::npos &&
         name.find('\\') == std::string::npos;
}

bool handler::FileProcessingNode::openFile(const std::filesystem::path &path,
                                           std::ios::openmode mode) {
  m_fileStream.open(path, mode);
  return m_fileStream.is_open();
}

void handler::FileProcessingNode::closeCurrentFile() {
  if (m_fileStream.is_open()) {
    m_fileStream.close();
  }
}

void handler::FileProcessingNode::upload(
    std::unique_ptr<FileUploadDescription> block,
    [[maybe_unused]] SessionPtr live_extend) {

  if (!block)
    return;

  if (!block->callback)
    return;

  /*if it is the end of file*/
  bool isEOF = (block->isEOF == std::string("1"));

  // redirect file stream
  auto sessionDescOpt = prepareUploadStream(*block);
  if (!sessionDescOpt.has_value()) {
    block->callback(ServiceStatus::FILE_OPEN_ERROR, 0);
    return;
  }

  auto sessionDesc = sessionDescOpt.value();

  try {
    // conduct base64 decode on block data first
    block->block_data = base64Decode(block->block_data);
    if (block->block_data.empty()) {
      spdlog::error("[{}]: Decoded block is empty. Skipping write.",
                    ServerConfig::get_instance()->GrpcServerName);
      block->callback(ServiceStatus::FILE_UPLOAD_ERROR, 0);
      return;
    }
  } catch (const std::exception &e) {
    spdlog::error("[{}]: base64 decoding failed: {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
    block->callback(ServiceStatus::FILE_UPLOAD_ERROR, 0);
    return;
  }

  const std::size_t actualBlockSize = block->block_data.size();

  if (block->transfered_size > sessionDesc.total_size) {
    spdlog::error("[{}]: Invalid offset for '{}': offset={}, server_total={}",
                  ServerConfig::get_instance()->GrpcServerName, block->filename,
                  block->transfered_size, sessionDesc.total_size);
    block->callback(ServiceStatus::FILE_UPLOAD_ERROR, 0);
    return;
  }

  if (actualBlockSize > static_cast<std::size_t>(sessionDesc.total_size -
                                                 block->transfered_size)) {
    spdlog::error("[{}]: Block overflow for '{}': offset={}, block_size={}, "
                  "server_total={}",
                  ServerConfig::get_instance()->GrpcServerName, block->filename,
                  block->transfered_size, actualBlockSize,
                  sessionDesc.total_size);
    block->callback(ServiceStatus::FILE_UPLOAD_ERROR, 0);
    return;
  }

  // write to file
  if (!writeToFile(block->block_data)) {
    spdlog::warn("[{}]: Skipped closing file due to write failure.",
                 ServerConfig::get_instance()->GrpcServerName);
    block->callback(ServiceStatus::FILE_WRITE_ERROR, 0);
    return;
  }

  // update progress
  block->transfered_size += block->block_data.size();

  if (!isEOF) {
    if (!FileHasherLogger::get_instance()->refresh(block->key, *block)) {
      spdlog::error(
          "[{}]: Failed to refresh redis upload record after write, key='{}'",
          ServerConfig::get_instance()->GrpcServerName, block->key);
      block->callback(ServiceStatus::REDIS_UNKOWN_ERROR, 0);
      return;
    }
  } else {
    spdlog::info("[{}]: EOF received, finalize upload for '{}'",
                 ServerConfig::get_instance()->GrpcServerName, block->filename);

    // when EOF, then remove KV value inside redis!
    if (!FileHasherLogger::get_instance()->remove(block->key)) {
      spdlog::error("[{}]: Failed to remove redis upload record, key='{}'",
                    ServerConfig::get_instance()->GrpcServerName, block->key);
      closeCurrentFile();
      block->callback(ServiceStatus::REDIS_UNKOWN_ERROR, 0);
      return;
    }

    closeCurrentFile();
  }

  block->callback(ServiceStatus::SERVICE_SUCCESS, block->transfered_size);
}

void handler::FileProcessingNode::download(
    std::unique_ptr<FileDownloadDescription> block,
    [[maybe_unused]] SessionPtr live_extend) {

  if (!block)
    return;

  if (!block->callback)
    return;

  /*if it is first package then we should create a new file*/
  bool isFirstPackage = block->curr_sequence == std::string("1");

  /*if it is the end of file*/
  bool isEOF = block->isEOF == std::string("1");

  // redirect file stream
  std::size_t total_size{};
  if (!prepareDownloadStream(block->filename, block->uuid,
                             block->transfered_size, total_size)) {
    block->callback(ServiceStatus::FILE_OPEN_ERROR, nullptr);
    return;
  }

  // write to file
  auto opt = readFromFile(block->current_block_size);
  if (!opt.has_value()) {
    spdlog::warn("[{}]: Skipped closing file due to read failure.",
                 ServerConfig::get_instance()->GrpcServerName);
    block->callback(ServiceStatus::FILE_READ_ERROR, nullptr);
  }

  std::string block_data = base64Encode(opt.value());
  std::string last_seq = isFirstPackage
                             ? std::to_string(calculateTotalSeqNumber(
                                   total_size, block->current_block_size))
                             : block->last_sequence;

  block->callback(
      ServiceStatus::SERVICE_SUCCESS,
      std::make_unique<typename FileDownloadDescription::DownloadInfo>(
          block_data, std::string{}, last_seq, total_size));
}

std::optional<handler::FileHasherDesc>
handler::FileProcessingNode::prepareUploadStream(
    const FileUploadDescription &block) {

  if (!validFilename(block.key)) {
    spdlog::error("[{}]: Illegal filename '{}'",
                  ServerConfig::get_instance()->GrpcServerName, block.filename);
    return std::nullopt;
  }

  std::filesystem::path output_dir =
      std::filesystem::path(ServerConfig::get_instance()->outputPath) /
      block.uuid;
  std::filesystem::path full_path = output_dir / block.key;

  std::error_code ec;
  std::filesystem::create_directories(output_dir, ec);
  if (ec) {
    spdlog::error("[{}]: Failed to create dir '{}': {}",
                  ServerConfig::get_instance()->GrpcServerName,
                  output_dir.string(), ec.message());
    return std::nullopt;
  }

  closeCurrentFile();

  // only check redis record for once
  auto serverDescOpt =
      FileHasherLogger::get_instance()->getFileDescBlock(block.key);
  bool hasActiveRecord = serverDescOpt.has_value();
  bool fileExists = std::filesystem::exists(full_path);

  /*
   *   No active redis record:
   *   1. stale local file exists -> timeout / invalid old upload
   *   2. no local file          -> completely new upload
   */
  if (!hasActiveRecord) {

    // Redis has no record, but local file exists (old timeout file / stale
    // upload remains)
    //  We must remove it and restart from beginning
    if (fileExists) {
      spdlog::warn("[{}]: No active redis record for key='{}', but old file "
                   "exists '{}'. Restart upload from zero.",
                   ServerConfig::get_instance()->GrpcServerName, block.key,
                   full_path.string());

      // client tries to resume, but server state already expired
      if (block.transfered_size != 0) {
        spdlog::error("[{}]: Upload state expired for '{}', client requested "
                      "offset={}, expected offset=0",
                      ServerConfig::get_instance()->GrpcServerName,
                      block.filename, block.transfered_size);

        return std::nullopt;
      }

      std::error_code remove_ec;
      std::filesystem::remove(full_path, remove_ec);

      if (remove_ec) {
        spdlog::error("[{}]: Failed to remove old file '{}': {}",
                      ServerConfig::get_instance()->GrpcServerName,
                      full_path.string(), remove_ec.message());

        return std::nullopt;
      }

      spdlog::info("[{}]: Stale file '{}' removed successfully.",
                   ServerConfig::get_instance()->GrpcServerName,
                   full_path.string());
    } else {
      // No redis record + no local file  => completely new upload
      if (block.transfered_size != 0) {
        spdlog::error(
            "[{}]: New upload '{}' requested invalid offset={}, expected 0",
            ServerConfig::get_instance()->GrpcServerName, block.filename,
            block.transfered_size);
        return std::nullopt;
      }

      spdlog::info("[{}]: New upload detected for key='{}', file='{}'.",
                   ServerConfig::get_instance()->GrpcServerName, block.key,
                   block.filename);
    }

    if (!openFile(full_path,
                  std::ios::binary | std::ios::out | std::ios::trunc)) {
      spdlog::error("[{}]: Failed to create file '{}'",
                    ServerConfig::get_instance()->GrpcServerName,
                    full_path.string());
      return std::nullopt;
    }

    m_fileStream.seekp(0, std::ios::beg);

    if (!m_fileStream) {
      spdlog::error("[{}]: Failed to seek new file '{}'",
                    ServerConfig::get_instance()->GrpcServerName,
                    full_path.string());

      closeCurrentFile();
      return std::nullopt;
    }

    if (!FileHasherLogger::get_instance()->insert(block)) {
      spdlog::error("[{}]: Failed to create redis upload record, key='{}'",
                    ServerConfig::get_instance()->GrpcServerName, block.key);

      closeCurrentFile();
      return std::nullopt;
    }

    spdlog::info(
        "[{}]: Upload stream prepared successfully for new upload '{}'",
        ServerConfig::get_instance()->GrpcServerName, block.filename);

    m_lastfile = block.filename;
    return block;
  }

  /*
   * Active redis record exists: try to resume upload from current offset
   */

  // Redis says upload is active, but local file is gone. This is an
  // inconsistent stale state; remove redis record.
  if (!fileExists) {
    spdlog::error("[{}]: Active upload record exists for key='{}', but local "
                  "file '{}' is missing. Remove stale redis record.",
                  ServerConfig::get_instance()->GrpcServerName, block.key,
                  full_path.string());

    FileHasherLogger::get_instance()->remove(block.key);
    return std::nullopt;
  }

  auto actual_size = std::filesystem::file_size(full_path, ec);
  if (ec) {
    spdlog::error("[{}]: Failed to query file size '{}': {}",
                  ServerConfig::get_instance()->GrpcServerName,
                  full_path.string(), ec.message());
    return std::nullopt;
  }

  if (block.transfered_size != actual_size) {
    spdlog::error(
        "[{}]: Upload offset mismatch for '{}': file_size={}, client_offset={}",
        ServerConfig::get_instance()->GrpcServerName, block.filename,
        actual_size, block.transfered_size);
    return std::nullopt;
  }

  if (!openFile(full_path, std::ios::binary | std::ios::in | std::ios::out)) {
    spdlog::error("[{}]: Failed to reopen file '{}'",
                  ServerConfig::get_instance()->GrpcServerName,
                  full_path.string());
    return std::nullopt;
  }

  m_fileStream.seekp(static_cast<std::streamoff>(block.transfered_size),
                     std::ios::beg);
  if (!m_fileStream) {
    spdlog::error("[{}]: Failed to seek file '{}' to offset={}",
                  ServerConfig::get_instance()->GrpcServerName,
                  full_path.string(), block.transfered_size);
    closeCurrentFile();
    return std::nullopt;
  }

  // Refresh redis active-upload record and TTL
  if (!FileHasherLogger::get_instance()->refresh(block.key, block)) {
    spdlog::error("[{}]: Failed to refresh redis upload record for key='{}'",
                  ServerConfig::get_instance()->GrpcServerName, block.key);

    closeCurrentFile();
    return std::nullopt;
  }

  spdlog::info("[{}]: File upload stream prepared successfully for key='{}', "
               "file='{}', offset={}",
               ServerConfig::get_instance()->GrpcServerName, block.key,
               block.filename, block.transfered_size);

  m_lastfile = block.filename;
  return *serverDescOpt.value();
}

bool handler::FileProcessingNode::prepareDownloadStream(
    const std::string &filename, const std::string &uuid, std::uint64_t offset,
    std::size_t &total_size) {

  if (!validFilename(filename)) {
    return false;
  }

  std::filesystem::path full_path =
      std::filesystem::path(ServerConfig::get_instance()->outputPath) / uuid /
      filename;

  std::error_code ec;
  if (!std::filesystem::exists(full_path)) {
    spdlog::error("[{}]: User '{}' Trys to Download File '{}' does not exist",
                  ServerConfig::get_instance()->GrpcServerName, uuid,
                  full_path.string());
    return false;
  }

  total_size = std::filesystem::file_size(full_path, ec);
  if (ec || offset > total_size) {
    spdlog::error("[{}]: Invalid download offset for '{}', expected={}, "
                  "however user request '{}'",
                  ServerConfig::get_instance()->GrpcServerName,
                  full_path.string(), offset, total_size);
    return false;
  }

  closeCurrentFile();

  if (!openFile(full_path, std::ios::binary | std::ios::in)) {
    spdlog::error("[{}]: Failed to open file '{}' for reading",
                  ServerConfig::get_instance()->GrpcServerName,
                  full_path.string());
    return false;
  }

  m_fileStream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!m_fileStream) {
    closeCurrentFile();
    return false;
  }

  m_lastfile = filename;
  return true;
}

bool handler::FileProcessingNode::writeToFile(const std::string &content) {
  // safety consideration
  try {
    if (m_fileStream.is_open()) {
      m_fileStream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      m_fileStream.write(content.data(), content.size());
      // m_fileStream.flush();
      return true;
    }
    spdlog::error("[{}]: File stream not open, cannot write to file",
                  ServerConfig::get_instance()->GrpcServerName);

  } catch (const std::ios_base::failure &e) {
    spdlog::error("[{}]: I/O error while writing to file: {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  } catch (const std::exception &e) {
    spdlog::error("[{}]: Unexpected error while writing to file: {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }
  return false;
}

std::optional<std::string>
handler::FileProcessingNode::readFromFile(const std::size_t max_size) {
  try {
    if (!m_fileStream.is_open()) {
      spdlog::error("[{}]: File stream not open, cannot read",
                    ServerConfig::get_instance()->GrpcServerName);
      return std::nullopt;
    }

    std::string buffer;
    buffer.resize(max_size);

    m_fileStream.read(buffer.data(), static_cast<std::streamsize>(max_size));
    std::streamsize bytes_read = m_fileStream.gcount();

    if (bytes_read <= 0) {
      return std::string(); // EOF
    }

    buffer.resize(static_cast<std::size_t>(bytes_read));
    return buffer;
  } catch (const std::ios_base::failure &e) {
    spdlog::error("[{}]: I/O error while reading: {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  } catch (const std::exception &e) {
    spdlog::error("[{}]: Unexpected error while reading: {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }

  return std::nullopt;
}

std::string
handler::FileProcessingNode::base64Decode(const std::string &origin) {
  std::string decoded;

  if (!absl::Base64Unescape(origin, &decoded)) {

    spdlog::error("[{}]: Base64 decode failed",
                  ServerConfig::get_instance()->GrpcServerName);
    return {};
  }

  return decoded;
}

std::string handler::FileProcessingNode::base64Encode(std::string_view origin) {

  std::string encoded;
  absl::Base64Escape(origin, &encoded);

  if (encoded.empty()) {
    spdlog::error("[{}]: Base64 encode failed",
                  ServerConfig::get_instance()->GrpcServerName);
    return {};
  }

  return encoded;
}

[[nodiscard]]
std::size_t handler::FileProcessingNode::calculateTotalSeqNumber(
    const std::size_t totalSize, const std::size_t chunkSize) {

  return static_cast<size_t>(
      std::ceil(static_cast<double>(totalSize) / chunkSize));
}
