#include <server/Session.hpp>
#include <server/UserManager.hpp>

UserManager::UserManager() {}

UserManager::~UserManager() { m_uuid2Session.clear(); }

std::optional<std::shared_ptr<Session>>
UserManager::getSession(const std::string &uuid) {
  std::lock_guard<std::mutex> _lckg(m_update_mtx);
  auto target = m_uuid2Session.find(uuid);
  if (target == m_uuid2Session.end()) {
    return std::nullopt;
  }
  return target->second;
}

void UserManager::removeUsrSession(const std::string &uuid) {
  std::lock_guard<std::mutex> _lckg(m_update_mtx);
  m_uuid2Session.erase(uuid);
}

void UserManager::alterUserSession(const std::string &uuid,
                                   std::shared_ptr<Session> session) {
  std::lock_guard<std::mutex> _lckg(m_update_mtx);
  m_uuid2Session.insert(
      std::pair<std::string, std::shared_ptr<Session>>(uuid, session));
}
