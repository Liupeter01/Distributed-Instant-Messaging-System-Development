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
                       std::size_t accumlated_size, std::size_t file_size)
      : filename(filename), block_data(block_data), checksum(checksum),
        curr_sequence(curr_sequence), last_sequence(last_sequence),
        accumlated_size(accumlated_size), file_size(file_size) {}

  std::string filename;
  std::string block_data;
  std::string checksum;
  std::string curr_sequence;
  std::string last_sequence;

  std::size_t accumlated_size;
  std::size_t file_size;
};

class FileProcessingNode : public Singleton<FileProcessingNode> {
  friend class Singleton<FileProcessingNode>;

  FileProcessingNode();

public:
  virtual ~FileProcessingNode();

public:
  void commit(std::unique_ptr<FileDescriptionBlock> block);
  void commit(const std::string &filename, const std::string &block_data,
              const std::string &checksum, const std::string &curr_sequence,
              const std::string &last_sequence, std::size_t accumlated_size,
              std::size_t file_size);

protected:
  [[nodiscard]] std::optional<std::filesystem::path>
  createFile(const std::string &filename);
  bool writeToFile(const std::string &content);
  bool resetFileStream(const std::string &filename,
                       const std::size_t cur_size = 0);

  [[nodiscard]] std::string base64Decode(const std::string &origin);

private:
  /*FileProcessingNode Class Operations*/
  void shutdown();
  void processing();
  void execute(std::unique_ptr<FileDescriptionBlock> block);

private:
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
  std::queue<std::unique_ptr<FileDescriptionBlock>> m_queue;
};
} // namespace handler

#endif
