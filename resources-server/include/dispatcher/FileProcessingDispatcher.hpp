#pragma once
#ifndef _FILE_PROCESSING_DISPATCHER_HPP_
#define _FILE_PROCESSING_DISPATCHER_HPP_
#include <algorithm>
#include <config/ServerConfig.hpp>
#include <handler/FileProcessingNode.hpp>
#include <memory>
#include <server/FileHasherLogger.hpp>
#include <server/Session.hpp>
#include <singleton/singleton.hpp>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_vector.h>

namespace dispatcher {

class FileProcessingDispatcher : public Singleton<FileProcessingDispatcher> {

  friend class Singleton<FileProcessingDispatcher>;
  using SessionPtr = std::shared_ptr<Session>;
  using FPTRType = std::shared_ptr<handler::FileProcessingNode>;
  using ContainerType = tbb::concurrent_vector<FPTRType>;

public:
  virtual ~FileProcessingDispatcher() { shutdown(); }
  void shutdown() {
    std::for_each(m_nodes.begin(), m_nodes.end(),
                  [](std::shared_ptr<handler::FileProcessingNode> &node) {
                    node->shutdown();
                  });
  }

  void commit(std::unique_ptr<handler::FileDescriptionBlock> block,
              [[maybe_unused]] SessionPtr live_extend) {

    if (!block)
      return;
    auto filename = block->filename;

    auto temp = dispatch_to_node(filename);
    if (!temp.has_value()) {
      spdlog::error("[{}]: Dispatch Error While Handling Session: {}",
                    ServerConfig::get_instance()->GrpcServerName, filename);
      return;
    }

    auto node = temp.value();
    node->commit(std::move(block), live_extend);

    // spdlog::info("[{}]: Dispatch Task To Node {} Successfully!",
    //           ServerConfig::get_instance()->GrpcServerName,
    //           node->getProcessingId());
  }

  void commit(const std::string &filename, const std::string &block_data,
              const std::string &checksum, const std::string &curr_sequence,
              const std::string &last_sequence, const std::string &_eof,
              std::size_t accumlated_size, std::size_t file_size,
              [[maybe_unused]] SessionPtr live_extend) {

    commit(std::make_unique<handler::FileDescriptionBlock>(
               filename, block_data, checksum, curr_sequence, last_sequence,
               _eof, accumlated_size, file_size),
           live_extend);
  }

protected:
  const std::size_t hash_to_index(std::string_view filename) const {
    return m_convertor(filename) % m_nodes.size();
  }

private:
  FileProcessingDispatcher()
      : FileProcessingDispatcher(std::thread::hardware_concurrency() < 2
                                     ? 2
                                     : std::thread::hardware_concurrency()) {}
  FileProcessingDispatcher(std::size_t threads) : m_convertor() {
    for (std::size_t thread = 0; thread < threads; ++thread) {
      m_nodes.emplace_back(
          std::make_shared<handler::FileProcessingNode>(thread));
    }
  }

  [[nodiscard]] typename ContainerType::iterator
  dispatch_to_iterator(std::string_view filename) {
    if (m_nodes.empty()) {
      return m_nodes.end(); // or throw
    }

    auto it = m_nodes.begin() + hash_to_index(filename);
    if (it != m_nodes.end()) {
      return it;
    }
    return m_nodes.end();
  }

  [[nodiscard]] std::optional<FPTRType>
  dispatch_to_node(std::string_view filename) {
    if (m_nodes.empty())
      return std::nullopt;

    try {
      return m_nodes.at(hash_to_index(filename));
    } catch (const std::exception &e) {
      spdlog::error("[{}]: Find File Processing Node Err, Reason:{}",
                    ServerConfig::get_instance()->GrpcServerName, e.what());
    }
    return std::nullopt;
  }

private:
  std::hash<std::string_view> m_convertor;
  ContainerType m_nodes;
};

} // namespace dispatcher

#endif
