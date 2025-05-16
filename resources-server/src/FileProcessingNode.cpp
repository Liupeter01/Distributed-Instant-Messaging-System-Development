#include <absl/strings/escaping.h> /*base64*/
#include <config/ServerConfig.hpp>
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

    auto &front = m_queue.front();
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
      spdlog::error(
          "[Resources Server]: Decoded block is empty. Skipping write.");
      return;
    }
  } catch (const std::exception &e) {
    spdlog::error("[Resources Server]: base64 decoding failed: {}", e.what());
    return;
  }

  // write to file
  if (!writeToFile(block.second->block_data)) {
    spdlog::warn(
        "[Resources Server]: Skipped closing file due to write failure.");
    return;
  }

  if (isEOF) {
    spdlog::info(
        "[Resources Server]: EOF received, file stream closed for '{}'",
        block.second->filename);
    closeCurrentFile();
  }
}

void handler::FileProcessingNode::commit(
    std::unique_ptr<FileDescriptionBlock> block,
    [[maybe_unused]] SessionPtr live_extend) {

  std::lock_guard<std::mutex> _lckg(m_mtx);
  if (m_queue.size() > ServerConfig::get_instance()->ResourceQueueSize) {
    spdlog::warn("[Resources Server]: FileProcessingNode {}'s Queue is full!",
                 processing_id);

    return;
  }

  spdlog::info("[Resources Server]: Commit File: {}", block->filename);
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

  if (validFilename(filename)) {
    spdlog::error("[Resources Server]: Illegal filename '{}'", filename);
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
        spdlog::error("[Resources Server]: Failed to reopen file '{}'",
                      filename);
        return false;
      }

      spdlog::info("[Resources Server]: Reopened file '{}', seeking to {}",
                   filename, cur_size);
      m_fileStream.seekp(cur_size, std::ios::beg);
      m_lastfile = filename;
      return true;
    }
  }

  // File doesn't exist ¡ª fall through to create
  spdlog::warn("[Resources Server]: File '{}' not found, will create new.",
               filename);

  // Create file path if not exist
  auto opt_path = resolveAndPreparePath(output_dir, filename);
  if (!opt_path) {
    return false;
  }

  // make sure stream is closed before reopening
  closeCurrentFile();

  if (!openFile(*opt_path, std::ios::binary | std::ios::trunc)) {
    spdlog::error("[Resources Server]: Failed to create and open new file '{}'",
                  filename);
    return false;
  }

  if (cur_size != 0) {
    spdlog::warn(
        "[Resources Server]: New file '{}', but cur_size = {}, seeking anyway.",
        filename, cur_size);
    m_fileStream.seekp(cur_size, std::ios::beg);
  }

  spdlog::info("[Resources Server]: Created and opened new file '{}'",
               filename);
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
      spdlog::error("[Resources Server]: Failed to create directories '{}': {}",
                    base.string(), ec.message());

      return std::nullopt;
    }
  }

  std::filesystem::path full = base / filename;
  std::filesystem::path target_path =
      std::filesystem::weakly_canonical(full, ec);

  if (ec) {
    spdlog::error(
        "[Resources Server]: Failed to create file '{}' in directory: {}",
        filename, ec.message());

    return std::nullopt;
  }
  spdlog::info("[Resources Server]: File path resolved successfully for '{}'",
               filename);
  return target_path;
}

bool handler::FileProcessingNode::writeToFile(const std::string &content) {
  // safety consideration
  try {
    if (m_fileStream.is_open()) {
      m_fileStream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      m_fileStream.write(content.data(), content.size());
      m_fileStream.flush();
      return true;
    } else {
      spdlog::warn(
          "[Resources Server]: File stream not open, cannot write to file");
    }

  } catch (const std::ios_base::failure &e) {
    spdlog::error("[Resources Server]: I/O error while writing to file: {}",
                  e.what());
  } catch (const std::exception &e) {
    spdlog::error(
        "[Resources Server]: Unexpected error while writing to file: {}",
        e.what());
  }
  return false;
}

std::string
handler::FileProcessingNode::base64Decode(const std::string &origin) {
  std::string decoded;
  absl::Base64Unescape(origin, &decoded);
  return decoded;
}
