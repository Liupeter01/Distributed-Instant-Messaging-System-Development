#include <absl/strings/escaping.h> /*base64*/
#include <config/ServerConfig.hpp>
#include <handler/FileProcessingNode.hpp>
#include <spdlog/spdlog.h>

handler::FileProcessingNode::FileProcessingNode()
          :FileProcessingNode(0)
{
}

handler::FileProcessingNode::FileProcessingNode(const std::size_t id) 
          : m_stop(false) 
          , processing_id(id){

          /*start processing thread to process queue*/
          m_working = std::thread(&FileProcessingNode::processing, this);
}

handler::FileProcessingNode::~FileProcessingNode() { shutdown(); }

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

void handler::FileProcessingNode::execute(
    std::unique_ptr<FileDescriptionBlock> block) {

  // redirect file stream
  resetFileStream(block->filename,
                  /*file pointer pos = */ block->accumlated_size);

  // conduct base64 decode on block data first
  block->block_data = base64Decode(block->block_data);

  // write to file
  writeToFile(block->block_data);
}

void handler::FileProcessingNode::commit(std::unique_ptr<FileDescriptionBlock> block, 
                                                                       [[maybe_unused]] SessionPtr live_extend) {

  spdlog::info("[Resources Server]: Commit File: {}", block->filename);

  m_queue.push(std::move(block));
  m_cv.notify_one();
}

void handler::FileProcessingNode::commit(const std::string &filename,
                                         const std::string &block_data,
                                         const std::string &checksum,
                                         const std::string &curr_sequence,
                                         const std::string &last_sequence,
                                         std::size_t accumlated_size,
                                         std::size_t file_size, [[maybe_unused]] SessionPtr live_extend) {

          commit(std::make_unique<FileDescriptionBlock>(filename, block_data, checksum,
                    curr_sequence, last_sequence,
                    accumlated_size, file_size), live_extend);
}

bool handler::FileProcessingNode::resetFileStream(const std::string &filename,
                                                  const std::size_t cur_size) {

  /*
   * check if we are going to operate the file consistently!
   * if the file is already opened, we don't need to open it again.
   */
  if (m_lastfile == filename) {
    return true;
  }

  m_lastfile = filename;

  // if m_lastfile is not equal to filename, we need to close the file stream
  if (m_fileStream.is_open()) {
    m_fileStream.close();
  }

  // Open another one, and test if it's already created before.
  m_fileStream.open(filename, std::ios::binary | std::ios::app);
  if (m_fileStream.is_open()) {
    spdlog::info("[Resources Server]: File Already Exist: {}, \
                   Current Write Position: {}",
                 filename, cur_size);

    // If the file is already exist, we need to seek according to cur_size
    m_fileStream.seekp(cur_size, std::ios::beg);
    return true;
  }

  spdlog::warn("[Resources Server]: File {} Not Exist, Creating a new one now!",
               filename);

  // Create the file check whether it exists or not.
  if (auto opt = createFile(filename); opt) {
    // This is the first time creating the file, so we open it in trunc mode
    m_fileStream.open(*opt, std::ios::binary | std::ios::trunc);
    return m_fileStream.is_open();
  }

  return false;
}

std::optional<std::filesystem::path>
handler::FileProcessingNode::createFile(const std::string &filename) {
          
          std::error_code ec;
  std::filesystem::path output_dir = ServerConfig::get_instance()->outputPath;
  std::filesystem::path full_path = output_dir / filename;

  // Ensure the output directory exists
  if (!std::filesystem::exists(output_dir)) {
            if (!std::filesystem::create_directories(output_dir, ec)) {
                      spdlog::error("[Resources Server]: Failed to create directories '{}': {}",
                                output_dir.string(), ec.message());

                      return std::nullopt;
            }
  }

  std::filesystem::path target_path =
      std::filesystem::weakly_canonical(full_path, ec);

  if (ec) {
    spdlog::error("[Resources Server]: Failed to create file '{}' in directory: {}",
              filename, ec.message());

    return std::nullopt;
  }
  spdlog::info("[Resources Server]: File path resolved successfully for '{}'", filename);
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
