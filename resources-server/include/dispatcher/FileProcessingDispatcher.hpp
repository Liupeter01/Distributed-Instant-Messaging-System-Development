#pragma once
#ifndef _FILE_PROCESSING_DISPATCHER_HPP_
#define _FILE_PROCESSING_DISPATCHER_HPP_
#include <memory>
#include <tbb/concurrent_vector.h>
#include <handler/FileProcessingNode.hpp>
#include <singleton/singleton.hpp>

namespace handler {
          class FileDescriptionBlock;
}

namespace dispatcher {

class FileProcessingDispatcher : public Singleton<FileProcessingDispatcher> {

  friend class Singleton<FileProcessingDispatcher>;
  using SessionPtr = std::shared_ptr<Session>;
  using FPTRType = std::shared_ptr<handler::FileProcessingNode>;
  using ContainerType =
            tbb::concurrent_vector<FPTRType>;

public:
  virtual ~FileProcessingDispatcher();
  void shutdown();

  void commit(std::unique_ptr<handler::FileDescriptionBlock> block,
            [[maybe_unused]] SessionPtr live_extend);

  void commit(const std::string& filename, const std::string& block_data,
            const std::string& checksum, const std::string& curr_sequence,
            const std::string& last_sequence, std::size_t accumlated_size,
            std::size_t file_size, [[maybe_unused]] SessionPtr live_extend);

protected:
          const std::size_t hash_to_index(std::string_view session_id) const;

private:
          FileProcessingDispatcher();
          FileProcessingDispatcher(std::size_t threads);

          [[nodiscard]] typename ContainerType::iterator 
          dispatch_to_iterator(std::string_view session_id);

          [[nodiscard]] std::optional<FPTRType>
         dispatch_to_node(std::string_view session_id);

private:
          std::hash<std::string_view> m_convertor;
          ContainerType m_nodes;
};

} // namespace dispatcher

#endif
