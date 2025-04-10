#include <algorithm>
#include <server/Session.hpp>
#include <server/UserManager.hpp>

UserManager::UserManager() {}

UserManager::~UserManager() { teminate(); }

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

          //safty consideration
          removeUsrSession(uuid);

          m_uuid2Session.insert(
                    std::pair<std::string, std::shared_ptr<Session>>(uuid, session));
}

void UserManager::teminate() {
          std::for_each(m_uuid2Session.begin(), m_uuid2Session.end(), [](std::shared_ptr<Session>& session) {
                    session->closeSession();
                    });
          m_uuid2Session.clear();
}