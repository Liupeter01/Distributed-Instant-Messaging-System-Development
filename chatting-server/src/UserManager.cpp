#include <network/def.hpp>
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

bool UserManager::removeUsrSession(const std::string &uuid) {

  typename ContainerType::accessor accessor;
  bool status = m_waitingToBeClosed.find(accessor, uuid);
  if (status) {
            /*remove the item from the container*/
            accessor->second->closeSession();
            m_waitingToBeClosed.erase(accessor);
  }
  return status;
}

bool  UserManager::removeUsrSession(const std::string& uuid,
                                                                const std::string& session_id) {

          typename ContainerType::accessor accessor;
          bool status = m_waitingToBeClosed.find(accessor, uuid);
          if (status) {
                    if (accessor->second->get_session_id() == session_id) {
                              /*remove the item from the container*/
                              accessor->second->closeSession();
                              m_waitingToBeClosed.erase(accessor);
                    }
          }
          return status;
}

void UserManager::createUserSession(const std::string& uuid, std::shared_ptr<Session> session) {
          m_uuid2Session.insert(
                    std::pair<std::string, std::shared_ptr<Session>>(uuid, session));
}

bool UserManager::moveUserToTerminationZone(const std::string& uuid) {
          typename ContainerType::accessor accessor;
          bool status = m_uuid2Session.find(accessor, uuid);
          if (status) {
                    {
                              typename ContainerType::accessor close_accessor;
                              m_waitingToBeClosed.insert(close_accessor, uuid);
                              close_accessor->second = std::move(accessor->second);
                    }
                    m_uuid2Session.erase(accessor); 
          }
          return status;
}

void UserManager::teminate() {

  std::for_each(m_uuid2Session.begin(), m_uuid2Session.end(),
                [this](decltype(*m_uuid2Session.begin()) &pair) {
                      pair.second->sendOfflineMessage();
                });
}