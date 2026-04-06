#pragma once
#ifndef _FILEHASHERLOGGER_HPP_
#define _FILEHASHERLOGGER_HPP_
#include <optional>
#include <singleton/singleton.hpp>
#include <string>
#include <tbb/concurrent_hash_map.h>

namespace handler {

struct FileHasherDesc {
  FileHasherDesc(const std::string &_filename, const std::string &_checksum,
                 const std::string &_curr_sequence,
                 const std::string &_last_sequence, const std::string &_eof,
                 std::size_t _transfered_size, std::size_t  _current_block_size, std::size_t _total_size)
      : filename(_filename), checksum(_checksum), curr_sequence(_curr_sequence),
        last_sequence(_last_sequence), transfered_size(_transfered_size),
            current_block_size(_current_block_size),
            total_size(_total_size), isEOF(_eof) {}

  std::string filename;
  std::string checksum;
  std::string curr_sequence;
  std::string last_sequence;
  std::string isEOF;

  std::size_t transfered_size;
  std::size_t current_block_size;
  std::size_t total_size;
};

struct FileDescriptionBlock {
  FileDescriptionBlock() = default;
  FileDescriptionBlock(const FileHasherDesc& o, const std::string& block)
            : FileDescriptionBlock(o.filename, block, o.checksum, o.curr_sequence,
                      o.last_sequence, o.isEOF, o.transfered_size,
                      o.current_block_size, o.total_size) {
  }

  FileDescriptionBlock(const std::string& _filename,
            const std::string& _block_data,
            const std::string& _checksum,
            const std::string& _curr_sequence,
            const std::string& _last_sequence,
            const std::string& _eof,
            std::size_t _transfered_size,
            std::size_t  _current_block_size,
            std::size_t _total_size)

            : filename(_filename), block_data(_block_data), checksum(_checksum),
            curr_sequence(_curr_sequence), last_sequence(_last_sequence),
            transfered_size(_transfered_size),
            current_block_size(_current_block_size),
            total_size(_total_size), isEOF(_eof) {
  }

  std::string filename;
  std::string block_data;
  std::string checksum;
  std::string curr_sequence;
  std::string last_sequence;
  std::string isEOF;

  std::size_t transfered_size;
  std::size_t current_block_size;
  std::size_t total_size;
};
} // namespace handler

class FileHasherLogger : public Singleton<FileHasherLogger> {
  friend class Singleton<FileHasherLogger>;
  friend class AsyncServer;
  using ContainerType = tbb::concurrent_hash_map<
      /*checksum data*/ std::string,
      /*file description block*/ std::shared_ptr<
          const handler::FileHasherDesc>>;

  FileHasherLogger() = default;

public:
  ~FileHasherLogger() = default;

public:
  void insert(const std::string &checksum,
              std::shared_ptr<const handler::FileHasherDesc> block) {
    typename ContainerType::accessor accessor;
    if (m_checksum2FileDesc.find(accessor, checksum)) {
      accessor->second.reset();
      accessor->second = block; // update!
      return;
    }

    m_checksum2FileDesc.insert(std::pair(checksum, block));
  }

  [[nodiscard]]
  std::optional<std::shared_ptr<const handler::FileHasherDesc>>
  getFileDescBlock(const std::string &checksum) {
    typename ContainerType::accessor accessor;
    if (!m_checksum2FileDesc.find(accessor, checksum))
      return std::nullopt;
    return accessor->second;
  }

private:
  ContainerType m_checksum2FileDesc;
};

#endif //_FILEHASHERLOGGER_HPP_
