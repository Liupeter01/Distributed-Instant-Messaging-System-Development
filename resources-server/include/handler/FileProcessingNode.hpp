#pragma once
#ifndef _FILE_PROCESSING_NODE_HPP_
#define _FILE_PROCESSING_NODE_HPP_
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <redis/RedisManager.hpp>
#include <server/FileHasherLogger.hpp>
#include <server/Session.hpp>
#include <singleton/singleton.hpp>
#include <thread>

namespace handler {
class FileProcessingNode {
  using SessionPtr = std::shared_ptr<Session>;

  using NodePtr = std::unique_ptr<FileHasherDesc>;
  using pair = std::pair<SessionPtr, NodePtr>;

  using RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool,
                                               redis::RedisContext>;

public:
  FileProcessingNode();
  FileProcessingNode(const std::size_t id);
  virtual ~FileProcessingNode();

public:
  void setProcessingId(const std::size_t id);
  const std::size_t getProcessingId() const;
  void shutdown();

  // we have to constraint the type
  template <typename T,
            typename std::enable_if<has_callback<T>::value, int>::type = 0>
  void commit(std::unique_ptr<T> block,
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

  static bool validFilename(std::string_view name);

protected:
  bool writeToFile(const std::string &content);
  std::optional<std::string> readFromFile(const std::size_t max_size);
  bool prepareUploadStream(const FileUploadDescription& block);

  bool prepareDownloadStream(const std::string &filename,
                             const std::string &uuid, std::uint64_t offset,
                             std::size_t &total_size);

  [[nodiscard]] static std::string base64Decode(const std::string &origin);
  [[nodiscard]] static std::string base64Encode(std::string_view origin);

  [[nodiscard]]
  static std::size_t calculateTotalSeqNumber(const std::size_t totalSize,
                                             const std::size_t chunkSize);

private:
  /*FileProcessingNode Class Operations*/
  void processing();
  void execute(pair &&block);

  bool openFile(const std::filesystem::path &path, std::ios::openmode mode);
  void closeCurrentFile();

  void upload(std::unique_ptr<FileUploadDescription> block,
              [[maybe_unused]] SessionPtr live_extend);

  void download(std::unique_ptr<FileDownloadDescription> block,
                [[maybe_unused]] SessionPtr live_extend);

private:
  std::size_t processing_id;

  /*file stream*/
  std::string m_lastfile;
  std::fstream m_fileStream;

  /*Server stop flag*/
  std::atomic<bool> m_stop;

  /*working thread, handling commited request*/
  std::thread m_working;

  /*mutex & cv => thread safety*/
  std::mutex m_mtx;
  std::condition_variable m_cv;

  /*user commit filedescription block to this processing node!*/
  std::queue<pair> m_queue;
};
} // namespace handler

#endif
