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
#include <server/Session.hpp>
#include <singleton/singleton.hpp>
#include <thread>

namespace handler {

struct FileDescriptionBlock {
  FileDescriptionBlock() = default;
  FileDescriptionBlock(const std::string &filename,
                       const std::string &block_data,
                       const std::string &checksum,
                       const std::string &curr_sequence,
                       const std::string &last_sequence,
                       const std::string &_eof, std::size_t accumlated_size,
                       std::size_t file_size)
      : filename(filename), block_data(block_data), checksum(checksum),
        curr_sequence(curr_sequence), last_sequence(last_sequence),
        accumlated_size(accumlated_size), file_size(file_size), isEOF(_eof) {}

  std::string filename;
  std::string block_data;
  std::string checksum;
  std::string curr_sequence;
  std::string last_sequence;
  std::string isEOF;

  std::size_t accumlated_size;
  std::size_t file_size;
};

class FileProcessingNode {
  using SessionPtr = std::shared_ptr<Session>;
  using NodePtr = std::unique_ptr<FileDescriptionBlock>;
  using pair = std::pair<SessionPtr, NodePtr>;

public:
  FileProcessingNode();
  FileProcessingNode(const std::size_t id);
  virtual ~FileProcessingNode();

public:
  void setProcessingId(const std::size_t id);
  const std::size_t getProcessingId() const;
  void shutdown();
  void commit(std::unique_ptr<FileDescriptionBlock> block,
              [[maybe_unused]] SessionPtr live_extend);
  void commit(const std::string &filename, const std::string &block_data,
              const std::string &checksum, const std::string &curr_sequence,
              const std::string &last_sequence, const std::string &_eof,
              std::size_t accumlated_size, std::size_t file_size,
              [[maybe_unused]] SessionPtr live_extend);

  static bool validFilename(std::string_view name);

protected:
  [[nodiscard]] std::optional<std::filesystem::path>
  resolveAndPreparePath(const std::filesystem::path &base,
                        const std::string &filename);
  bool writeToFile(const std::string &content);
  bool resetFileStream(const bool isFirstPackage, const std::string &filename,
                       const std::size_t cur_size = 0);

  [[nodiscard]] std::string base64Decode(const std::string &origin);

private:
  /*FileProcessingNode Class Operations*/
  void processing();
  void execute(pair &&block);

  bool openFile(const std::filesystem::path &path, std::ios::openmode mode);
  void closeCurrentFile();

private:
  std::size_t processing_id;

  /*file stream*/
  std::string m_lastfile;
  std::ofstream m_fileStream;

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
