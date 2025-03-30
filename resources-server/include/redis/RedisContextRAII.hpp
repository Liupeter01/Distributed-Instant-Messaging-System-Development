#pragma once
#ifndef _REDISCONTEXTRAII_HPP_
#define _REDISCONTEXTRAII_HPP_
#include <string>
#include <string_view>
#include <tools/tools.hpp>

namespace redis {
class RedisContext {
  friend class RedisReply;

  /*also remove copy ctor*/
  RedisContext(const RedisContext &) = delete;
  RedisContext(RedisContext &&) = delete;

  RedisContext &operator=(const RedisContext &) = delete;
  RedisContext &operator=(RedisContext &&) = delete;

public:
  ~RedisContext() = default;
  RedisContext() noexcept;

  /*connect to redis automatically*/
  RedisContext(const std::string &ip, unsigned short port,
               const std::string &password) noexcept;

  /*RedisTools will shutdown connection automatically!*/
  void close() = delete;

  bool isValid();
  bool checkError();
  bool checkAuth(std::string_view sv);
  bool setValue(const std::string &key, const std::string &value);
  bool setValue2Hash(const std::string &key, const std::string &field,
                     const std::string &value);
  bool delValueFromHash(const std::string &key, const std::string &field);
  bool leftPush(const std::string &key, const std::string &value);
  bool rightPush(const std::string &key, const std::string &value);
  bool delPair(const std::string &key);
  bool existKey(const std::string &key);

  std::optional<std::string> checkValue(const std::string &key);
  std::optional<std::string> leftPop(const std::string &key);
  std::optional<std::string> rightPop(const std::string &key);
  std::optional<std::string> getValueFromHash(const std::string &key,
                                              const std::string &field);

  std::optional<tools::RedisContextWrapper> operator->();

private:
  /*if check error failed, m_valid will be set to false*/
  bool m_valid;

  /*redis context*/
  tools::RedisSmartPtr<redisContext> m_redisContext;
};
} // namespace redis

#endif // !_REDISCONTEXTRAII_HPP_
