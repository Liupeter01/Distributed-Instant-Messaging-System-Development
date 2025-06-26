#include <ChattingHistory.hpp>
#include <QJsonArray>
#include <useraccountmanager.hpp>

UserAccountManager::UserAccountManager() : m_info() {}

UserAccountManager::~UserAccountManager() { clear(); }

void UserAccountManager::appendFriendRequestList(const QJsonArray &array) {
  for (const QJsonValue &obj : array) {
    addItem2List(std::make_shared<UserFriendRequest>(
        obj["src_uuid"].toString(), obj["dst_uuid"].toString(),
        obj["nickname"].toString(), obj["message"].toString(),
        obj["avator"].toString(), obj["username"].toString(),
        obj["description"].toString(), static_cast<Sex>(obj["sex"].toInt())));
  }
}

void UserAccountManager::appendAuthFriendList(const QJsonArray &array) {
  for (const QJsonValue &obj : array) {
    addItem2List(std::make_shared<UserNameCard>(
        obj["uuid"].toString(), obj["avator"].toString(),
        obj["username"].toString(), obj["nickname"].toString(),
        obj["description"].toString(), static_cast<Sex>(obj["sex"].toInt())));
  }
}

void UserAccountManager::appendArrayToList(TargetList target,
                                           const QJsonArray &array) {
  if (target == TargetList::FRIENDLIST) {
    appendAuthFriendList(array);
  } else if (target == TargetList::REQUESTLIST) {
    appendFriendRequestList(array);
  }
}

void UserAccountManager::addItem2List(std::shared_ptr<UserFriendRequest> info) {
  m_friend_request_list.push_back(info);
}

void UserAccountManager::addItem2List(std::shared_ptr<UserNameCard> info) {
  m_auth_friend_list[info->m_uuid] = info;
}

void UserAccountManager::addItem2List(
    const QString &friend_uuid, std::shared_ptr<FriendChattingHistory> info) {
  m_user_chatting_histroy[friend_uuid] = info;
}

std::optional<std::shared_ptr<FriendChattingHistory>>
UserAccountManager::getChattingHistoryFromList(const QString &friend_uuid) {
  if (alreadyExistInHistoryList(friend_uuid)) {
    return m_user_chatting_histroy[friend_uuid];
  }
  return std::nullopt;
}

std::vector<std::shared_ptr<UserFriendRequest>>
UserAccountManager::getFriendRequestList() {
  return m_friend_request_list;
}

std::optional<std::vector<std::shared_ptr<UserFriendRequest>>>
UserAccountManager::getFriendRequestList(std::size_t &begin,
                                         const std::size_t interval) {

  /* user requested number even larger then greatest amount */
  if (begin < 0 || begin >= m_friend_request_list.size()) {
    /*return a empty container*/
    return std::nullopt;
  }

  std::vector<std::shared_ptr<UserFriendRequest>> list;

  /*updated it to the acceptable size of the list*/
  auto it_begin = m_friend_request_list.begin();
  auto it_end = it_begin;

  std::advance(it_begin, begin);

  /*
   * begin is lower than the greated amount
   * However, begin + interval is larger than greated amount
   * So we will need to set begin to the greatest amount, indicating list
   * reading done!
   */
  if (begin + interval >= m_friend_request_list.size()) {
    it_end = m_friend_request_list.end();

    /*updated it to the whole size of the list*/
    begin = m_friend_request_list.size();
  } else {
    std::advance(it_end, begin + interval);

    /*updated begin with interval*/
    begin += interval;
  }

  std::copy(it_begin, it_end, std::back_inserter(list));
  return list;
}

std::vector<std::shared_ptr<UserNameCard>>
UserAccountManager::getAuthFriendList() {
  std::vector<std::shared_ptr<UserNameCard>> list;
  std::transform(m_auth_friend_list.begin(), m_auth_friend_list.end(),
                 std::back_inserter(list),
                 [](const auto &T) { return T.second; });
  return list;
}

std::optional<std::vector<std::shared_ptr<UserNameCard>>>
UserAccountManager::getAuthFriendList(std::size_t &begin,
                                      const std::size_t interval) {

  /* user requested number even larger then greatest amount */
  if (begin < 0 || begin >= m_auth_friend_list.size()) {

    /*return a empty container*/
    return std::nullopt;
  }

  std::vector<std::shared_ptr<UserNameCard>> list;

  /*updated it to the acceptable size of the list*/
  auto it_begin = m_auth_friend_list.begin();
  auto it_end = it_begin;

  std::advance(it_begin, begin);

  /*
   * begin is lower than the greated amount
   * However, begin + interval is larger than greated amount
   * So we will need to set begin to the greatest amount, indicating list
   * reading done!
   */
  if (begin + interval > m_auth_friend_list.size()) {
    it_end = m_auth_friend_list.end();

    /*updated it to the whole size of the list*/
    begin = m_auth_friend_list.size();
  } else {
    std::advance(it_end, begin + interval);

    /*updated begin with interval*/
    begin += interval;
  }

  std::transform(it_begin, it_end, std::back_inserter(list),
                 [](const auto &T) { return T.second; });
  return list;
}

std::optional<std::shared_ptr<UserNameCard>>
UserAccountManager::findAuthFriendsInfo(const QString &uuid) {
  if (!alreadyExistInAuthList(uuid)) {
    return std::nullopt;
  }
  return m_auth_friend_list.find(uuid)->second;
}

bool UserAccountManager::alreadyExistInAuthList(const QString &uuid) const {
  return m_auth_friend_list.find(uuid) != m_auth_friend_list.end();
}

bool UserAccountManager::alreadyExistInRequestList(const QString &uuid) const {
  auto it =
      std::find_if(m_friend_request_list.begin(), m_friend_request_list.end(),
                   [uuid](std::shared_ptr<UserFriendRequest> item) {
                     // uuid should equal to m_uuid(from_uuid)
                     return item->sender_card.m_uuid == uuid;
                   });

  return it != m_friend_request_list.end();
}

bool UserAccountManager::alreadyExistInHistoryList(
    const QString &friend_uuid) const {
  return m_user_chatting_histroy.find(friend_uuid) !=
         m_user_chatting_histroy.end();
}

void UserAccountManager::setUserInfo(std::shared_ptr<UserNameCard> info) {
  m_userInfo = info;
}
void UserAccountManager::clear() {
  m_info.clear();
  m_userInfo.reset();
  m_friend_request_list.clear();
  m_auth_friend_list.clear();
  m_user_chatting_histroy.clear();
}
