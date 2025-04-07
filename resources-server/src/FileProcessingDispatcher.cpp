#include <algorithm>
#include <dispatcher/FileProcessingDispatcher.hpp>
#include <spdlog/spdlog.h>

dispatcher::FileProcessingDispatcher::FileProcessingDispatcher()
    : FileProcessingDispatcher(std::thread::hardware_concurrency() < 2
                                   ? 2
                                   : std::thread::hardware_concurrency()) {}

dispatcher::FileProcessingDispatcher::FileProcessingDispatcher(
    std::size_t threads)
    : m_convertor() {
  for (std::size_t thread = 0; thread < threads; ++thread) {
    m_nodes.emplace_back(std::make_shared<handler::FileProcessingNode>(thread));
  }
}

dispatcher::FileProcessingDispatcher::~FileProcessingDispatcher() {
  shutdown();
}

void dispatcher::FileProcessingDispatcher::shutdown() {
  std::for_each(m_nodes.begin(), m_nodes.end(),
                [](std::shared_ptr<handler::FileProcessingNode> &node) {
                  node->shutdown();
                });
}

void dispatcher::FileProcessingDispatcher::commit(
    std::unique_ptr<handler::FileDescriptionBlock> block,
    [[maybe_unused]] SessionPtr live_extend) {

  auto filename = block->filename;

  // if opt has value then it could be executed by this if condition
  if (auto opt = dispatch_to_node(filename); opt) {
    (*opt)->commit(std::move(block), live_extend);
    spdlog::info(
        "[Resources Server]: Dispatcher File Processing Task To Node {} "
        "Successfully!",
        (*opt)->getProcessingId());
    return;
  }

  spdlog::error(
      "[Resources Server]: Dispatcher Error While Handling Session: {}",
      filename);
}

void dispatcher::FileProcessingDispatcher::commit(
    const std::string &filename, const std::string &block_data,
    const std::string &checksum, const std::string &curr_sequence,
    const std::string &last_sequence, const std::string& _eof, std::size_t accumlated_size,
    std::size_t file_size, [[maybe_unused]] SessionPtr live_extend) {

  commit(std::make_unique<handler::FileDescriptionBlock>(
             filename, block_data, checksum, curr_sequence, last_sequence,_eof,
             accumlated_size, file_size),
         live_extend);
}

const std::size_t dispatcher::FileProcessingDispatcher::hash_to_index(
    std::string_view filename) const {
  return m_convertor(filename) % m_nodes.size();
}

dispatcher::FileProcessingDispatcher::ContainerType::iterator
dispatcher::FileProcessingDispatcher::dispatch_to_iterator(
    std::string_view filename) {
  if (m_nodes.empty()) {
    return m_nodes.end(); // or throw
  }

  auto it = m_nodes.begin() + hash_to_index(filename);
  if (it != m_nodes.end()) {
            return it;
  }
  return m_nodes.end();
}

std::optional<typename dispatcher::FileProcessingDispatcher::FPTRType>
dispatcher::FileProcessingDispatcher::dispatch_to_node(
    std::string_view filename) {
  if (m_nodes.empty())
    return std::nullopt;

  try{
           return m_nodes.at(hash_to_index(filename));
  }
  catch (const std::exception&e ){
                      spdlog::error("[Resources Server]: Retrieve File Processing Node Error, Reason:{}", e.what());
  }
  return std::nullopt;
}
