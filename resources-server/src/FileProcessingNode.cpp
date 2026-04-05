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

void handler::FileProcessingNode::execute(pair &&block) {

  /*if it is first package then we should create a new file*/
  bool isFirstPackage = block.second->curr_sequence == std::string("1");

  /*if it is the end of file*/
  bool isEOF = block.second->isEOF == std::string("1");

  // redirect file stream
  if (!resetFileStream(isFirstPackage, block.second->filename,
                       block.second->accumlated_size)) {
    return;
  }

  try {
    // conduct base64 decode on block data first
    block.second->block_data = base64Decode(block.second->block_data);
    if (block.second->block_data.empty()) {
      spdlog::error("[{}]: Decoded block is empty. Skipping write.",
                    ServerConfig::get_instance()->GrpcServerName);
      return;
    }
  } catch (const std::exception &e) {
    spdlog::error("[{}]: base64 decoding failed: {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
    return;
  }

  // write to file
  if (!writeToFile(block.second->block_data)) {
    spdlog::warn("[{}]: Skipped closing file due to write failure.",
                 ServerConfig::get_instance()->GrpcServerName);
    return;
  }

  if (isEOF) {
    spdlog::info("[{}]: EOF received, file stream closed for '{}'",
                 ServerConfig::get_instance()->GrpcServerName,
                 block.second->filename);
    closeCurrentFile();
  }
}

void handler::FileProcessingNode::commit(
    std::unique_ptr<FileDescriptionBlock> block,
    [[maybe_unused]] SessionPtr live_extend) {

  std::lock_guard<std::mutex> _lckg(m_mtx);
  // if (m_queue.size() > ServerConfig::get_instance()->ResourceQueueSize) {
  //   spdlog::warn("[{}]: FileProcessingNode {}'s Queue is full!",
  //             ServerConfig::get_instance()->GrpcServerName, processing_id);
  //   return;
  // }
  //  spdlog::info("[{}]: Commit File: {}",
  //  ServerConfig::get_instance()->GrpcServerName, block->filename);
  m_queue.push(std::make_pair(live_extend, std::move(block)));
  m_cv.notify_one();
}

void handler::FileProcessingNode::commit(
    const std::string &filename, const std::string &block_data,
    const std::string &checksum, const std::string &curr_sequence,
    const std::string &last_sequence, const std::string &_eof,
    std::size_t accumlated_size, std::size_t file_size,
    [[maybe_unused]] SessionPtr live_extend) {

  commit(std::make_unique<FileDescriptionBlock>(
             filename, block_data, checksum, curr_sequence, last_sequence, _eof,
             accumlated_size, file_size),
         live_extend);
}

bool handler::FileProcessingNode::resetFileStream(const bool isFirstPackage,
                                                  const std::string &filename,
                                                  const std::size_t cur_size) {

  if (!validFilename(filename)) {
    spdlog::error("[{}]: Illegal filename '{}'",
                  ServerConfig::get_instance()->GrpcServerName, filename);
    return false;
  }

  std::filesystem::path output_dir = ServerConfig::get_instance()->outputPath;
  std::filesystem::path full_path = output_dir / filename;

  // If not first package, try to reuse stream
  if (!isFirstPackage) {
    if (m_lastfile == filename && m_fileStream.is_open()) {
      return true;
    }

    // if m_lastfile is not equal to filename, we need to close the file stream
    closeCurrentFile();

    // Open another file, and test if it's already created before.
    if (std::filesystem::exists(full_path)) {
      if (!openFile(full_path, std::ios::binary | std::ios::app)) {
        spdlog::error("[{}]: Failed to reopen file '{}'",
                      ServerConfig::get_instance()->GrpcServerName, filename);
        return false;
      }

      spdlog::info("[{}]: Reopened file '{}', seeking to {}",
                   ServerConfig::get_instance()->GrpcServerName, filename,
                   cur_size);
      m_fileStream.seekp(cur_size, std::ios::beg);
      m_lastfile = filename;
      return true;
    }
  }

  // File doesn't exist ¡ª fall through to create
  spdlog::warn("[{}]: File '{}' not found, will create new.",
            ServerConfig::get_instance()->GrpcServerName, filename);

  // Create file path if not exist
  auto opt_path = resolveAndPreparePath(output_dir, filename);
  if (!opt_path) {
    return false;
  }

  // make sure stream is closed before reopening
  closeCurrentFile();

  if (!openFile(*opt_path, std::ios::binary | std::ios::trunc)) {
    spdlog::error("[{}]: Failed to create and open new file '{}'",
                  ServerConfig::get_instance()->GrpcServerName, filename);
    return false;
  }

  /*
  * cur_size means how many bytes of this file WILL BE TRANSFERED, instead of have been transfered!
  * 
  -   if (cur_size != 0) {
  -         spdlog::error("[{}]: New file '{}', but cur_size = {}, aborting.",
  -                   ServerConfig::get_instance()->GrpcServerName, filename, cur_size);
  -         closeCurrentFile();
  -         return false;
  - }
  *
  */

  m_fileStream.seekp(0, std::ios::beg);

  spdlog::info("[{}]: Created and opened new file '{}'",
               ServerConfig::get_instance()->GrpcServerName, filename);

  m_lastfile = filename;
  return true;
}

std::optional<std::filesystem::path>
handler::FileProcessingNode::resolveAndPreparePath(
    const std::filesystem::path &base, const std::string &filename) {

  std::error_code ec;

  // Ensure the output directory exists
  if (!std::filesystem::exists(base)) {
    if (!std::filesystem::create_directories(base, ec)) {
      spdlog::error("[{}]: Failed to create directories '{}': {}",
                    ServerConfig::get_instance()->GrpcServerName, base.string(),
                    ec.message());

      return std::nullopt;
    }
  }

  std::filesystem::path full = base / filename;
  std::filesystem::path target_path =
      std::filesystem::weakly_canonical(full, ec);

  if (ec) {
    spdlog::error("[{}]: Failed to create file '{}' in directory: {}",
                  ServerConfig::get_instance()->GrpcServerName, filename,
                  ec.message());

    return std::nullopt;
  }
  spdlog::info("[{}]: File path resolved successfully for '{}'",
               ServerConfig::get_instance()->GrpcServerName, filename);
  return target_path;
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
    spdlog::warn("[{}]: File stream not open, cannot write to file",
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
