#pragma once
#ifndef _USERMANAGER_HPP_
#define _USERMANAGER_HPP_
#include <optional>
#include <singleton/singleton.hpp>
#include <string>
#include <tbb/concurrent_hash_map.h>

/*declaration*/
class Session;

class UserManager : public Singleton<UserManager> {
  friend class Singleton<UserManager>;
  UserManager();

  using ContainerType = tbb::concurrent_hash_map<
            std::string,
            /*user belonged session*/ std::shared_ptr<Session>>;

public:
  ~UserManager();
  std::optional<std::shared_ptr<Session>> getSession(const std::string &uuid);
  void removeUsrSession(const std::string &uuid);
  void alterUserSession(const std::string &uuid,
                        std::shared_ptr<Session> session);

protected:
          void teminate();

private:
          ContainerType m_uuid2Session;
};
#endif //_USERMANAGER_HPP_
