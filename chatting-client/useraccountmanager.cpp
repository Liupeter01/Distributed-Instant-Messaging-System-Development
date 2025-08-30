#include <QJsonArray>
#include <QJsonValue>
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

  if (target == TargetList::FRIENDLIST)
    appendAuthFriendList(array);
  else if (target == TargetList::REQUESTLIST)
    appendFriendRequestList(array);
}

void UserAccountManager::addItem2List(std::shared_ptr<UserFriendRequest> info) {
  m_friend_request_list.push_back(info);
}

// add friend usernamecard to auth friend list
void UserAccountManager::addItem2List(std::shared_ptr<UserNameCard> info) {
  if (!m_auth_friend_list.count(info->m_uuid))
    m_auth_friend_list[info->m_uuid] = info;
}

// create mapping relation of uuid<->thread_id
// and relation between thread_id <-> ChattingThreadDesc
void UserAccountManager::addItem2List(
    const QString &friend_uuid, std::shared_ptr<ChattingThreadDesc> info) {

  QString thread_id = QString::fromStdString(info->getThreadId());

  if(!m_threadDescLists.count(thread_id) &&
      !m_friendOnThreadsLists.count(friend_uuid) ){

        // relation between thread_id <-> ChattingThreadDesc
        m_threadDescLists[thread_id] = info;

          // relation between user uuid <-> thread_id
        m_friendOnThreadsLists[friend_uuid] = thread_id;

        //add to session overall list!
        m_allChattingSessions.push_back(thread_id);
  }

}

void UserAccountManager::addItem2List(std::shared_ptr<UserChatThread> info) {

    auto thread_id = info->getCurChattingThreadId();
    auto friend_uuid = info->getUserNameCard()->m_uuid;
    auto type = info->getUserChatType();

    //add auth friend
    addItem2List(info->getUserNameCard());

    // create mapping relation of uuid<->thread_id
    // and relation between thread_id <-> ChattingThreadDesc
    if(type == UserChatType::GROUP){
         addItem2List(friend_uuid, std::make_shared<ChattingThreadDesc>(
                                      ChattingThreadDesc::createGroupChat(thread_id.toStdString())));

    }
    else{
        addItem2List(friend_uuid, std::make_shared<ChattingThreadDesc>(
            ChattingThreadDesc::createPrivateChat(
                thread_id.toStdString(),
                m_userInfo->m_uuid.toStdString(),
                friend_uuid.toStdString())
        ));
    }

  if (!m_ThreadData.count(thread_id)) {
    m_ThreadData[thread_id] = info;
  }
}

std::optional<std::shared_ptr<UserChatThread>>
UserAccountManager::getChattingThreadData(const QString &thread_id) {

  if (!m_ThreadData.count(thread_id))
    return std::nullopt;

  return m_ThreadData[thread_id];
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

std::optional<QString>
UserAccountManager::getThreadIdByUUID(const QString &uuid) {

  if (!m_friendOnThreadsLists.count(uuid)) {
    return std::nullopt;
  }
  return m_friendOnThreadsLists[uuid];
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

void UserAccountManager::clear() {
  m_info.clear();
  m_userInfo.reset();
  m_friend_request_list.clear();
  m_auth_friend_list.clear();
  m_ThreadData.clear();
}

UserAccountManager::ChattingServerInfo::ChattingServerInfo()
    : uuid(), host(), port(), token() {}

void UserAccountManager::ChattingServerInfo::clear() {
  uuid.clear();
  host.clear();
  port.clear();
  token.clear();
}

std::optional<std::shared_ptr<UserChatThread> >
UserAccountManager::_loadSession(const std::size_t pos)
{
          if (pos >= m_allChattingSessions.size()) {
                    return std::nullopt;
          }
          auto thread_id = m_allChattingSessions[pos];

          auto it = m_ThreadData.find(thread_id);

          //Not Found!
          if (it == m_ThreadData.end()) {
                    return std::nullopt;
          }
          return it->second;
}

std::optional<std::shared_ptr<UserChatThread>>
UserAccountManager::getCurThreadSession() {
    return _loadSession(m_currSessionLoadingSeq);
}

std::optional<std::shared_ptr<UserChatThread> >
UserAccountManager::getNextThreadSession(){
    ++m_currSessionLoadingSeq;
    return _loadSession(m_currSessionLoadingSeq);
}