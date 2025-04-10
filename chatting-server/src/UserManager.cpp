#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <network/def.hpp>
#include <server/UserManager.hpp>

std::string UserManager::redis_server_login = "redis_server";

/*store user base info in redis*/
std::string UserManager::user_prefix = "user_info_";

/*store the server name that this user belongs to*/
std::string UserManager::server_prefix = "uuid_";

UserManager::UserManager() {}

UserManager::~UserManager() { m_uuid2Session.clear(); }

std::optional<std::shared_ptr<Session>>
UserManager::getSession(const std::string &uuid) {

  typename ContainerType::const_accessor accessor;
  if (m_uuid2Session.find(accessor, uuid)) {
    return accessor->second;
  }
  return std::nullopt;
}

void UserManager::removeUsrSession(const std::string &uuid) {
  typename ContainerType::accessor accessor;
  if (m_uuid2Session.find(accessor, uuid)) {
    /*remove the item from the container*/
    accessor->second->closeSession();
    m_uuid2Session.erase(accessor);
  }
}

void UserManager::alterUserSession(const std::string &uuid,
                                   std::shared_ptr<Session> session) {

  // safty consideration
  removeUsrSession(uuid);

  m_uuid2Session.insert(
      std::pair<std::string, std::shared_ptr<Session>>(uuid, session));
}

void UserManager::teminate() {

  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  std::for_each(m_uuid2Session.begin(), m_uuid2Session.end(),
                [this, &raii](decltype(*m_uuid2Session.begin()) &pair) {
                  UserManager::kick(raii, pair.second);
                });

  m_uuid2Session.clear();
}

void UserManager::kick(RedisRAII &raii, std::shared_ptr<Session> session) {

  boost::json::object logout;
  logout["error"] = static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS);
  logout["uuid"] = session->get_user_uuid();

  session->sendMessage(ServiceType::SERVICE_LOGOUTRESPONSE,
                       boost::json::serialize(logout));

  /*Remove from Redis*/
  raii->get()->delPair(server_prefix + session->get_user_uuid());

  session->closeSession();
}
