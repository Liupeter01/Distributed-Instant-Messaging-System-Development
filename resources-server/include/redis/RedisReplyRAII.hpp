#pragma once
#ifndef _REDISREPLYRAII_HPP_
#define _REDISREPLYRAII_HPP_
#include <redis/RedisContextRAII.hpp>
#include <tools/tools.hpp>

namespace redis {
class RedisReply {
public:
  ~RedisReply() = default;
  RedisReply() noexcept : m_redisReply(nullptr) {}
  /*also remove copy ctor*/
  RedisReply(const RedisReply &) = delete;
  RedisReply(RedisReply &&) = delete;

  RedisReply &operator=(const RedisReply &) = delete;
  RedisReply &operator=(RedisReply &&) = delete;

  template <typename... Args>
  bool redisCommand(RedisContext &context, const std::string &command,
                    Args &&...args) {
    m_redisReply.reset(reinterpret_cast<redisReply *>(
        ::redisCommand(context.m_redisContext.get(), command.c_str(),
                       std::forward<Args>(args)...)));
    return isSuccessful();
  }

public:
  std::optional<long long> getInterger() const;
  std::optional<int> getType() const;
  std::optional<std::string> getMessage() const;

private:
  bool isSuccessful() const;

private:
  tools::RedisSmartPtr<redisReply> m_redisReply;
};
} // namespace redis

#endif // !_REDISREPLYRAII_HPP_
