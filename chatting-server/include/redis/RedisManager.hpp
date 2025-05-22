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

  RedisConnectionPool() noexcept;
  RedisConnectionPool(const std::size_t _timeout, 
                                        const std::string& _ip, 
                                        const  std::string& _passwd, 
                                        const unsigned short _port) noexcept;

public:
  ~RedisConnectionPool() = default;

protected:
          void roundRobinChecking();

private:
          bool connector(const std::string &_ip, const  std::string& _passwd, const unsigned short _port);

private:
          /*redis connector*/
          std::string m_ip;
          std::string m_passwd;
          unsigned short m_port;

          /*round robin thread*/
          std::size_t m_timeout;
          std::thread m_RRThread;

};
} // namespace redis
#endif
