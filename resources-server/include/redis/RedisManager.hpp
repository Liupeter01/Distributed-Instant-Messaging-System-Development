#pragma once
#ifndef _REDISMANAGER_HPP_
#define _REDISMANAGER_HPP_
#include <config/ServerConfig.hpp>
#include <redis/RedisReplyRAII.hpp>
#include <service/ConnectionPool.hpp>
#include <spdlog/spdlog.h>

namespace redis {
class RedisConnectionPool
    : public connection::ConnectionPool<RedisConnectionPool,
                                        redis::RedisContext> {
  using self = RedisConnectionPool;
  using context = redis::RedisContext;
  using context_ptr = std::unique_ptr<context>;
  friend class Singleton<RedisConnectionPool>;

  RedisConnectionPool() {
    spdlog::info("Connecting to Redis service ip: {0}, port: {1}",
                 ServerConfig::get_instance()->Redis_ip_addr.c_str(),
                 ServerConfig::get_instance()->Redis_port);

    for (std::size_t i = 0; i < m_queue_size; ++i) {
      m_stub_queue.push(std::move(std::make_unique<context>(
          ServerConfig::get_instance()->Redis_ip_addr,
          ServerConfig::get_instance()->Redis_port,
          ServerConfig::get_instance()->Redis_passwd)));
    }
  }

public:
  ~RedisConnectionPool() = default;
};
} // namespace redis
#endif
