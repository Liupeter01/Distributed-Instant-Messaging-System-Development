#pragma once
#ifndef _USERMANAGER_HPP_
#define _USERMANAGER_HPP_
#include <optional>
#include <redis/RedisManager.hpp>
#include <server/Session.hpp>
#include <service/ConnectionPool.hpp>
#include <singleton/singleton.hpp>
#include <string>
#include <tbb/concurrent_hash_map.h>

class AsyncServer;

class UserManager : public Singleton<UserManager> {
  friend class Singleton<UserManager>;
  friend class AsyncServer;
  UserManager();

  using RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool,
                                               redis::RedisContext>;
  using ContainerType = tbb::concurrent_hash_map<
      std::string,
      /*user belonged session*/ std::shared_ptr<Session>>;

public:
  ~UserManager();
  std::optional<std::shared_ptr<Session>> getSession(const std::string &uuid);
  bool removeUsrSession(const std::string &uuid);
  bool removeUsrSession(const std::string &uuid, const std::string &session_id);
  void createUserSession(const std::string &uuid,
                         std::shared_ptr<Session> session);

  /*we do not need to teminate the user at once, we just put them to another
   * structure*/
  bool moveUserToTerminationZone(const std::string &uuid);

protected:
  void teminate();
  void eraseWaitingSession(const std::string &uuid);

private:
  bool m_status;
  ContainerType m_uuid2Session;
  ContainerType m_waitingToBeClosed;
};
#endif //_USERMANAGER_HPP_
