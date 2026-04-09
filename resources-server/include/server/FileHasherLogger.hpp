#pragma once
#ifndef _FILEHASHERLOGGER_HPP_
#define _FILEHASHERLOGGER_HPP_
#include <optional>
#include <singleton/singleton.hpp>
#include <string>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <functional>
#include <network/def.hpp>

/* 
 * we no longer gonna to leave it at server's memory (tbb/concurrent_hash_map)
 * we will choose redis instead!
*/
#include <redis/RedisManager.hpp>

namespace handler {

struct FileHasherDesc {
          FileHasherDesc(const std::string& _filename, const std::string& _checksum, const std::string& _filePath,
                    const std::string& _curr_sequence,
                    const std::string& _last_sequence, const std::string& _eof,
                    std::size_t _transfered_size, std::size_t  _current_block_size, std::size_t _total_size);

  std::string key; //=filename_checksum

  std::string filename;
  std::string filePath;
  std::string checksum;
  std::string curr_sequence;
  std::string last_sequence;
  std::string isEOF;

  std::size_t transfered_size;
  std::size_t current_block_size;
  std::size_t total_size;
};

struct FileDescriptionBlock : public FileHasherDesc {
          FileDescriptionBlock(const FileHasherDesc& o,
                    const std::string& block,
                    std::function<void(const ServiceStatus)>&& _callback);

  std::string block_data;
  std::function<void(const ServiceStatus)> callback;
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
          bool insert(std::shared_ptr<const handler::FileHasherDesc> block) {
                    RedisRAII raii;
                    boost::json::object src_obj;
                    src_obj["key"] = block->key;
                    src_obj["filename"] = block->filename;
                    src_obj["checksum"] = block->checksum;
                    src_obj["filepath"] = block->filePath;
                    src_obj["last_seq"] = block->last_sequence;
                    src_obj["cur_seq"] = block->curr_sequence;
                    src_obj["transfered_size"] = block->transfered_size;
                    src_obj["current_block_size"] = block->current_block_size;
                    src_obj["total_size"] = block->total_size;
                    src_obj["EOF"] = block->isEOF;

                    return raii->get()->setValueExp(block->key, boost::json::serialize(src_obj), 3000);
          }

  [[nodiscard]]
  std::optional<std::shared_ptr<const handler::FileHasherDesc>>
            getFileDescBlock(const std::string& key) {

            RedisRAII raii;

            if (key.empty()) {
                      return {};
            }

            //Search For Info Cache in Redis
            auto opt = raii->get()->checkValue(key);
            if (!opt.has_value()) {
                      return {};
            }

            /*parse cache data inside Redis*/
            try {
                      boost::json::object obj = boost::json::parse(opt.value()).as_object();

                      return std::make_shared<const handler::FileHasherDesc>(
                                boost::json::value_to<std::string>(obj["filename"]),
                                boost::json::value_to<std::string>(obj["checksum"]),
                                boost::json::value_to<std::string>(obj["filepath"]),
                                boost::json::value_to<std::string>(obj["cur_seq"]),
                                boost::json::value_to<std::string>(obj["last_seq"]),
                                boost::json::value_to<std::string>(obj["EOF"]),
                                obj["transfered_size"].as_uint64(),
                                obj["current_block_size"].as_uint64(),
                                obj["total_size"].as_uint64());
            }
            catch (const boost::json::system_error& e) {
                      spdlog::error("Failed to parse json data!");
                      return std::nullopt;
            }
  }
};

#endif //_FILEHASHERLOGGER_HPP_
