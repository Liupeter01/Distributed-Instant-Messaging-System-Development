#pragma once
#ifndef _USERMANAGER_HPP_
#define _USERMANAGER_HPP_
#include <string>
#include <optional>
#include <server/Session.hpp>
#include <singleton/singleton.hpp>
#include <tbb/concurrent_hash_map.h>
#include <redis/RedisManager.hpp>
#include <service/ConnectionPool.hpp>

class UserManager : public Singleton<UserManager> {
  friend class Singleton<UserManager>;
  UserManager();

  using  RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>;
  using ContainerType = tbb::concurrent_hash_map<
            std::string,
            /*user belonged session*/ std::shared_ptr<Session>>;

public:
  ~UserManager();
  std::optional<std::shared_ptr<Session>> getSession(const std::string &uuid);
  void removeUsrSession(const std::string &uuid);
  void alterUserSession(const std::string &uuid,
                        std::shared_ptr<Session> session);

  static void kick(RedisRAII& raii, std::shared_ptr<Session> session);

protected:
          void teminate();

private:
          /*redis*/
          static std::string redis_server_login;

          /*store user base info in redis*/
          static std::string user_prefix;

          /*store the server name that this user belongs to*/
          static std::string server_prefix;

          ContainerType m_uuid2Session;
};
#endif //_USERMANAGER_HPP_
