#pragma once
#ifndef _FILEHASHERLOGGER_HPP_
#define _FILEHASHERLOGGER_HPP_
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <functional>
#include <network/def.hpp>
#include <optional>
#include <singleton/singleton.hpp>
#include <string>
#include <tools/tools.hpp>

/*
 * we no longer gonna to leave it at server's memory (tbb/concurrent_hash_map)
 * we will choose redis instead!
 */
#include <redis/RedisManager.hpp>

/*check weather a callback variable exist or not*/
template <typename, typename = void> struct has_callback : std::false_type {};

template <typename T>
struct has_callback<
    T, std::void_t<decltype(std::declval<std::decay_t<T>>().callback)>>
    : std::true_type {};

namespace handler {

enum class TransferDirection { Download, Upload };

struct FileHasherDesc {

  FileHasherDesc(const std::string &_uuid, const std::string &_filename,
                 const std::string &_checksum, const std::string &_filePath,
                 const std::string &_curr_sequence,
                 const std::string &_last_sequence, const std::string &_eof,
                 std::size_t _transfered_size, std::size_t _current_block_size,
                 std::size_t _total_size,
                 TransferDirection _direction = TransferDirection::Upload)
      : uuid(_uuid), filename(_filename), checksum(_checksum),
        filePath(_filePath), curr_sequence(_curr_sequence),
        last_sequence(_last_sequence), transfered_size(_transfered_size),
        current_block_size(_current_block_size), total_size(_total_size),
        isEOF(_eof), key(_checksum + std::string("_") + _filename),
        direction(_direction) {
    key = _checksum + std::string("_") + _filename;
  }

  std::string key; //=checksum_filename

  std::string uuid;

  std::string filename;
  std::string filePath;
  std::string checksum;
  std::string curr_sequence;
  std::string last_sequence;
  std::string isEOF;

  std::size_t transfered_size;
  std::size_t current_block_size;
  std::size_t total_size;

  TransferDirection direction = TransferDirection::Upload;
};

struct FileDownloadDescription : public FileHasherDesc {
  struct DownloadInfo {
    DownloadInfo(std::string block_data_, std::string checksum_,
                 std::string last_seq_, std::size_t total_size_)
        : block_data(std::move(block_data_)), checksum(std::move(checksum_)),
          last_seq(std::move(last_seq_)),
          total_size(std::to_string(total_size_)) {}
    std::string block_data;
    std::string checksum;
    std::string last_seq;
    std::string total_size;
  };

  using Func =
      std::function<void(const ServiceStatus, std::unique_ptr<DownloadInfo>)>;

  /*only for download mode*/
  FileDownloadDescription(const FileHasherDesc &o, Func &&_callback)
      : FileHasherDesc(o), callback(std::move(_callback)) {}

  operator FileHasherDesc() const { return *this; }

  Func callback;
};

struct FileUploadDescription : public FileHasherDesc {
  using Func = std::function<void(const ServiceStatus, const std::size_t)>;

  /*only for upload mode*/
  FileUploadDescription(const FileHasherDesc &o, const std::string &block,
                        Func &&_callback)
      : FileHasherDesc(o), block_data(block), callback(std::move(_callback)) {}

  operator FileHasherDesc() const { return *this; }

  std::string block_data;
  Func callback;
};

} // namespace handler

class FileHasherLogger : public Singleton<FileHasherLogger> {
  friend class Singleton<FileHasherLogger>;
  friend class AsyncServer;
  FileHasherLogger() = default;

  using RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool,
                                               redis::RedisContext>;

public:
  ~FileHasherLogger() = default;

public:
  bool insert(const std::shared_ptr<handler::FileHasherDesc> &block) {
    if (!block) {
      spdlog::error("[{}]: insert failed, block is null",
                    ServerConfig::get_instance()->GrpcServerName);
      return false;
    }
    return insert(*block);
  }

  bool insert(const handler::FileHasherDesc &block,
              const std::size_t expSec = 3) {
    if (block.key.empty()) {
      spdlog::error("[{}]: insert failed, empty redis key",
                    ServerConfig::get_instance()->GrpcServerName);
      return false;
    }

    RedisRAII raii;
    auto payload = serializeBlock(block);

    bool ok = raii->get()->setValueExp(block.key, payload, expSec);
    if (!ok) {
      spdlog::error("[{}]: insert redis record failed, key='{}'",
                    ServerConfig::get_instance()->GrpcServerName, block.key);
    }
    return ok;
  }

  bool remove(const std::string &key) {
    if (key.empty()) {
      spdlog::warn("[{}]: remove skipped, empty key",
                   ServerConfig::get_instance()->GrpcServerName);
      return false;
    }

    RedisRAII raii;
    bool ok = raii->get()->delPair(key);
    if (!ok) {
      spdlog::warn("[{}]: remove redis key='{}' failed or key not found",
                   ServerConfig::get_instance()->GrpcServerName, key);
    }
    return ok;
  }

  bool refresh(const std::string &key,
               const std::shared_ptr<handler::FileHasherDesc> &block,
               const std::size_t expSec = 10) {
    if (!block) {
      spdlog::error("[{}]: refresh failed, block is null, key='{}'",
                    ServerConfig::get_instance()->GrpcServerName, key);
      return false;
    }
    return refresh(key, *block, expSec);
  }

  bool refresh(const std::string &key, const handler::FileHasherDesc &block,
               const std::size_t expSec = 10) {

    if (key.empty()) {
      spdlog::error("[{}]: refresh failed, empty key",
                    ServerConfig::get_instance()->GrpcServerName);
      return false;
    }

    RedisRAII raii;
    auto payload = serializeBlock(block);

    bool ok = raii->get()->setValueExp(key, payload, expSec);
    if (!ok) {
      spdlog::error("[{}]: refresh redis record failed, key='{}'",
                    ServerConfig::get_instance()->GrpcServerName, key);
    }
    return ok;
  }

  [[nodiscard]]
  std::optional<std::shared_ptr<handler::FileHasherDesc>>
  getFileDescBlock(const std::string &key) {

    if (key.empty()) {
      spdlog::warn("[{}]: getFileDescBlock skipped, empty key",
                   ServerConfig::get_instance()->GrpcServerName);
      return std::nullopt;
    }

    RedisRAII raii;

    // Search For Info Cache in Redis
    auto opt = raii->get()->checkValue(key);
    if (!opt.has_value()) {
      return std::nullopt;
    }

    /*parse cache data inside Redis*/
    try {
      boost::json::value jv = boost::json::parse(opt.value());
      boost::json::object obj = jv.as_object();

      std::string uuid = tools::getString(obj, "uuid");
      std::string filename = tools::getString(obj, "filename");
      std::string checksum = tools::getString(obj, "checksum");
      std::string filepath = tools::getString(obj, "filepath");
      std::string lastSeq = tools::getString(obj, "last_seq");
      std::string curSeq = tools::getString(obj, "cur_seq");
      std::string eof = tools::getString(obj, "EOF");

      std::int64_t transferedSize = tools::getInt64(obj, "transfered_size");
      std::int64_t currentBlockSize =
          tools::getInt64(obj, "current_block_size");
      std::int64_t totalSize = tools::getInt64(obj, "total_size");

      return std::make_shared<handler::FileHasherDesc>(
          uuid, filename, checksum, filepath, curSeq, lastSeq, eof,
          static_cast<int>(transferedSize), static_cast<int>(currentBlockSize),
          static_cast<int>(totalSize));

    } catch (const boost::json::system_error &e) {
      spdlog::error("[{}]: Failed to parse redis json for key='{}', reason={}",
                    ServerConfig::get_instance()->GrpcServerName, key,
                    e.what());
      return std::nullopt;
    } catch (const std::exception &e) {
      spdlog::error("[{}]: Failed to build FileHasherDesc from redis for "
                    "key='{}', reason={}",
                    ServerConfig::get_instance()->GrpcServerName, key,
                    e.what());
      return std::nullopt;
    }
  }

private:
  static std::string serializeBlock(const handler::FileHasherDesc &block) {
    boost::json::object obj;
    obj["uuid"] = block.uuid;
    obj["key"] = block.key;
    obj["filename"] = block.filename;
    obj["checksum"] = block.checksum;
    obj["filepath"] = block.filePath;
    obj["last_seq"] = block.last_sequence;
    obj["cur_seq"] = block.curr_sequence;
    obj["transfered_size"] = block.transfered_size;
    obj["current_block_size"] = block.current_block_size;
    obj["total_size"] = block.total_size;
    obj["EOF"] = block.isEOF;

    return boost::json::serialize(obj);
  }
};

#endif //_FILEHASHERLOGGER_HPP_
