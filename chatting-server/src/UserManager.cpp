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
          if (!m_waitingToBeClosed.find(accessor, uuid))
                    return false;

          auto session = accessor->second;

          //Mark as delete this session later
          //maybe it still has some other commitments to do
          session->markAsDeferredTerminated([this, uuid, session]() {
                    session->closeSession();
                    eraseWaitingSession(uuid);
                    });

          return true;
}

bool UserManager::removeUsrSession(const std::string &uuid,
                                   const std::string &session_id) {

  typename ContainerType::accessor accessor;
  if (!m_waitingToBeClosed.find(accessor, uuid)) 
            return false;

  auto session = accessor->second;
  if (session->get_session_id() != session_id) {
            spdlog::warn("[{}] removeUsrSession called with mismatched session_id for UUID {}", 
                      ServerConfig::get_instance()->GrpcServerName, uuid);
            return false;
  }

  //Mark as delete this session later
  //maybe it still has some other commitments to do
  session->markAsDeferredTerminated([this, uuid, session]() {
            session->closeSession();
            eraseWaitingSession(uuid);
            });

  return true;
}

void UserManager::eraseWaitingSession(const std::string& uuid) {
          typename ContainerType::accessor erase_accessor;
          if (m_waitingToBeClosed.find(erase_accessor, uuid)) {
                    m_waitingToBeClosed.erase(erase_accessor);
          }
}

void UserManager::createUserSession(const std::string &uuid,
                                    std::shared_ptr<Session> session) {
  m_uuid2Session.insert(
      std::pair<std::string, std::shared_ptr<Session>>(uuid, session));
}

bool UserManager::moveUserToTerminationZone(const std::string &uuid) {

  typename ContainerType::accessor accessor;
  if (!m_uuid2Session.find(accessor, uuid))
            return false;

  typename ContainerType::accessor close_accessor;
  m_waitingToBeClosed.insert(close_accessor, uuid);
  close_accessor->second = std::move(accessor->second);
  m_uuid2Session.erase(accessor);
  return true;
}

void UserManager::teminate() {

  std::for_each(m_uuid2Session.begin(), m_uuid2Session.end(),
                [this](decltype(*m_uuid2Session.begin()) &pair) {
                  pair.second->sendOfflineMessage();
                });
}
