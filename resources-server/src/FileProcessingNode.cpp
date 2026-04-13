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

    //auto &front = m_queue.front();
    execute(std::move(m_queue.front()));
    m_queue.pop();
  }
}

void handler::FileProcessingNode::execute(pair&& block) {
          if (block.second->direction == TransferDirection::Download) {
                    download(tools::static_unique_ptr_cast<FileDownloadDescription>(std::move(block.second)), block.first);
          }
          else if (block.second->direction == TransferDirection::Upload) {
                    upload(tools::static_unique_ptr_cast<FileUploadDescription>(std::move(block.second)), block.first);
          }
          else {
                    spdlog::error("[{}]: Invalid TransferDirection. It could only be Download or Transfer",
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

void handler::FileProcessingNode::upload(std::unique_ptr<FileUploadDescription> block,
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
          if (!prepareUploadStream(block->filename, block->uuid, block->transfered_size)) {
                    block->callback(ServiceStatus::FILE_OPEN_ERROR);
                    return;
          }

          try {
                    // conduct base64 decode on block data first
                    block->block_data = base64Decode(block->block_data);
                    if (block->block_data.empty()) {
                              spdlog::error("[{}]: Decoded block is empty. Skipping write.",
                                        ServerConfig::get_instance()->GrpcServerName);

                              return;
                    }
          }
          catch (const std::exception& e) {
                    spdlog::error("[{}]: base64 decoding failed: {}",
                              ServerConfig::get_instance()->GrpcServerName, e.what());
                    block->callback(ServiceStatus::FILE_UPLOAD_ERROR);
                    return;
          }

          // write to file
          if (!writeToFile(block->block_data)) {
                    spdlog::warn("[{}]: Skipped closing file due to write failure.",
                              ServerConfig::get_instance()->GrpcServerName);
                    block->callback(ServiceStatus::FILE_WRITE_ERROR);
                    return;
          }

          if (isEOF) {
                    spdlog::info("[{}]: EOF received, file stream closed for '{}'",
                              ServerConfig::get_instance()->GrpcServerName,
                              block->filename);
                    closeCurrentFile();
          }

          block->callback(ServiceStatus::SERVICE_SUCCESS);
}

void handler::FileProcessingNode::download(std::unique_ptr<FileDownloadDescription> block,
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
          if (!prepareDownloadStream(block->filename, block->uuid, block->transfered_size, total_size)) {
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
          std::string last_seq = isFirstPackage ? std::to_string(calculateTotalSeqNumber(total_size, block->current_block_size)) : block->last_sequence;

          block->callback(ServiceStatus::SERVICE_SUCCESS, 
                    std::make_unique<typename FileDownloadDescription::DownloadInfo>(
                              block_data,
                              std::string{},
                              last_seq,
                              total_size)
          );
}

bool handler::FileProcessingNode::prepareUploadStream(
          const std::string& filename,
          const std::string& uuid,
          std::uint64_t offset) {

          if (!validFilename(filename)) {
                    spdlog::error("[{}]: Illegal filename '{}'",
                              ServerConfig::get_instance()->GrpcServerName, filename);
                    return false;
          }

          std::filesystem::path output_dir =
                    std::filesystem::path(ServerConfig::get_instance()->outputPath) / uuid;
          std::filesystem::path full_path = output_dir / filename;

          std::error_code ec;
          std::filesystem::create_directories(output_dir, ec);
          if (ec) {
                    spdlog::error("[{}]: Failed to create dir '{}': {}",
                              ServerConfig::get_instance()->GrpcServerName,
                              output_dir.string(), ec.message());
                    return false;
          }

          closeCurrentFile();

          /*File Not Exist*/
          if (!std::filesystem::exists(full_path)) {

                    //new file, but offset is not zero??
                    if (offset != 0) {
                              spdlog::error("[{}]: Resume upload requested for missing file '{}', offset={}",
                                        ServerConfig::get_instance()->GrpcServerName,
                                        filename, offset);
                              return false;
                    }

                    if (!openFile(full_path, std::ios::binary | std::ios::out | std::ios::trunc)) {
                              spdlog::error("[{}]: Failed to create file '{}'",
                                        ServerConfig::get_instance()->GrpcServerName,
                                        full_path.string());
                              return false;
                    }

                    m_fileStream.seekp(0, std::ios::beg);
                    m_lastfile = filename;
                    return true;
          }

          auto actual_size = std::filesystem::file_size(full_path, ec);
          if (ec) {
                    spdlog::error("[{}]: Failed to query file size '{}': {}",
                              ServerConfig::get_instance()->GrpcServerName,
                              full_path.string(), ec.message());
                    return false;
          }

          if (actual_size != offset) {
                    spdlog::error("[{}]: Upload offset mismatch for '{}': expected={}, however user request '{}'",
                              ServerConfig::get_instance()->GrpcServerName,
                              filename, actual_size, offset);
                    return false;
          }

          if (!openFile(full_path, std::ios::binary | std::ios::in | std::ios::out)) {
                    spdlog::error("[{}]: Failed to reopen file '{}'",
                              ServerConfig::get_instance()->GrpcServerName,
                              full_path.string());
                    return false;
          }

          m_fileStream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
          if (!m_fileStream) {
                    spdlog::error("[{}]: Failed to seek file '{}'",
                              ServerConfig::get_instance()->GrpcServerName,
                              full_path.string());
                    closeCurrentFile();
                    return false;
          }

          m_lastfile = filename;
          return true;
}

bool handler::FileProcessingNode::prepareDownloadStream(
          const std::string& filename,
          const std::string& uuid,
          std::uint64_t offset,
          std::size_t& total_size) {

          if (!validFilename(filename)) {
                    return false;
          }

          std::filesystem::path full_path =
                    std::filesystem::path(ServerConfig::get_instance()->outputPath) / uuid / filename;

          std::error_code ec;
          if (!std::filesystem::exists(full_path)) {
                    spdlog::error("[{}]: User '{}' Trys to Download File '{}' does not exist",
                              ServerConfig::get_instance()->GrpcServerName,
                              uuid,
                              full_path.string());
                    return false;
          }

          total_size = std::filesystem::file_size(full_path, ec);
          if (ec || offset > total_size) {
                    spdlog::error("[{}]: Invalid download offset for '{}', expected={}, however user request '{}'",
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
                              return std::string();  // EOF
                    }

                    buffer.resize(static_cast<std::size_t>(bytes_read));
                    return buffer;
          }
          catch (const std::ios_base::failure& e) {
                    spdlog::error("[{}]: I/O error while reading: {}",
                              ServerConfig::get_instance()->GrpcServerName, e.what());
          }
          catch (const std::exception& e) {
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
std::size_t handler::FileProcessingNode::calculateTotalSeqNumber(const std::size_t totalSize,
          const std::size_t chunkSize) {

          return static_cast<size_t>(
                    std::ceil(static_cast<double>(totalSize) / chunkSize));
}