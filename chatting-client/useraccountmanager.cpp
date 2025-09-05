#include <QJsonArray>
#include <QJsonValue>
#include <useraccountmanager.hpp>

UserAccountManager::UserAccountManager() : m_info() {}

UserAccountManager::~UserAccountManager() { clear(); }

void UserAccountManager::appendFriendRequestList(const QJsonArray &array) {
  for (const QJsonValue &v : array) {
    if (!v.isObject())
      continue;
    const auto obj = v.toObject();
    addItem2List(std::make_shared<UserFriendRequest>(
        obj["src_uuid"].toString(), obj["dst_uuid"].toString(),
        obj["nickname"].toString(), obj["message"].toString(),
        obj["avator"].toString(), obj["username"].toString(),
        obj["description"].toString(), static_cast<Sex>(obj["sex"].toInt())));
  }
}

void UserAccountManager::appendAuthFriendList(const QJsonArray &array) {
  for (const QJsonValue &v : array) {
    if (!v.isObject())
      continue;
    const auto obj = v.toObject();
    addItem2List(std::make_shared<UserNameCard>(
        obj["uuid"].toString(), obj["avator"].toString(),
        obj["username"].toString(), obj["nickname"].toString(),
        obj["description"].toString(), static_cast<Sex>(obj["sex"].toInt())));
  }
}

void UserAccountManager::appendArrayToList(TargetList target,
                                           const QJsonArray &array) {

  /*
   * DO NOT use the lock at here!!!
   * Lock are exist in appendAuthFriendList/appendFriendRequestList functions
   */
  if (target == TargetList::FRIENDLIST)
    appendAuthFriendList(array);
  else if (target == TargetList::REQUESTLIST)
    appendFriendRequestList(array);
}

void UserAccountManager::addItem2List(std::shared_ptr<UserFriendRequest> info) {
  if (!info)
    return;
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_friend_request_list.push_back(info);
}

// add friend usernamecard to auth friend list
void UserAccountManager::addItem2List(std::shared_ptr<UserNameCard> info) {
  if (!info)
    return;
  if (info->m_uuid.isEmpty())
    return;

  std::lock_guard<std::mutex> _lckg(m_mtx);
  auto [_, status] = m_auth_friend_list.try_emplace(info->m_uuid, info);
  if (!status) {
    qDebug() << "add Usernamecard to List already exist!\n";
  }
}

// create mapping relation of uuid<->thread_id
// and relation between thread_id <-> ChattingThreadDesc
void UserAccountManager::addItem2List(
    const QString &friend_uuid, std::shared_ptr<ChattingThreadDesc> info) {

  if (!info)
    return;
  QString thread_id = QString::fromStdString(info->getThreadId());
  if (thread_id.isEmpty())
    return;

  /*acquire the lock here! we have to make sure the all process are unique*/
  std::lock_guard<std::mutex> _lckg(m_mtx);

  // relation between thread_id <-> ChattingThreadDesc
  auto [desc_iterator, threadDescStatus] =
      m_threadDescLists.try_emplace(thread_id, info);
  if (!threadDescStatus) {
    qDebug() << "add ChattingThreadDesc to Map already exist!\n";
    return;
  }

  // relation between user uuid <-> thread_id
  auto [friend2thread_iterator, friendMapThreadStatus] =
      m_friendOnThreadsLists.try_emplace(friend_uuid, thread_id);
  if (!friendMapThreadStatus) {
    qDebug() << "friend uuid already exist! remove this chat thread in early "
                "stage\n";
    m_threadDescLists.erase(thread_id);
    return;
  }

  // add to session overall list!
  m_allChattingSessions.push_back(thread_id);
}

void UserAccountManager::addItem2List(std::shared_ptr<UserChatThread> info) {

  if (!info || !m_userInfo || !info->getUserNameCard())
    return;

  const QString thread_id = info->getCurChattingThreadId();
  auto namecard = info->getUserNameCard();
  const QString friend_uuid = info->getUserNameCard()->m_uuid;
  const auto type = info->getUserChatType();

  // create mapping relation of uuid<->thread_id
  // and relation between thread_id <-> ChattingThreadDesc
  auto thread_desc =
      (type == UserChatType::GROUP)
          ? std::make_shared<ChattingThreadDesc>(
                ChattingThreadDesc::createGroupChat(thread_id.toStdString()))
          : std::make_shared<ChattingThreadDesc>(
                ChattingThreadDesc::createPrivateChat(
                    thread_id.toStdString(), m_userInfo->m_uuid.toStdString(),
                    friend_uuid.toStdString()));

  /*acquire the lock here! we have to make sure the all process are unique*/
  std::lock_guard<std::mutex> _lckg(m_mtx);

  // add auth friend(we remove the pervious addItem2List overload)
  // because all process should be controlled by one lock!
  auto [auth_iterator, authFriendStatus] =
      m_auth_friend_list.try_emplace(friend_uuid, namecard);
  if (!authFriendStatus) {
    qDebug() << "friend uuid already exist!\n";
    return;
  }

  // relation between thread_id <-> ChattingThreadDesc
  auto [desc_iterator, threadDescStatus] =
      m_threadDescLists.try_emplace(thread_id, thread_desc);
  if (!threadDescStatus) {
    // rollback
    qDebug() << "add ChattingThreadDesc to Map already exist!\n";
    m_auth_friend_list.erase(friend_uuid);
    return;
  }

  // thread_id <->std::shared_ptr<UserChatThread>
  auto [thread2Chatthread_iterator, threadid2ChatThreadStatus] =
      m_ThreadData.try_emplace(thread_id, info);
  if (!threadid2ChatThreadStatus) {

    // rollback
    qDebug() << "thread id mapping with chatthread already exist\n";
    m_auth_friend_list.erase(friend_uuid);
    m_threadDescLists.erase(thread_id);
    return;
  }

  // relation between user uuid <-> thread_id
  auto [friend2thread_iterator, friendMapThreadStatus] =
      m_friendOnThreadsLists.try_emplace(friend_uuid, thread_id);
  if (!friendMapThreadStatus) {
    // rollback
    qDebug() << "friend uuid already exist!\n";
    m_auth_friend_list.erase(friend_uuid);
    m_threadDescLists.erase(thread_id);
    m_ThreadData.erase(thread_id);
    return;
  }

  // add to session overall list!
  m_allChattingSessions.push_back(thread_id);
}

std::optional<std::shared_ptr<UserChatThread>>
UserAccountManager::getChattingThreadData(const QString &thread_id) {

  std::lock_guard<std::mutex> _lckg(m_mtx);
  auto it = m_ThreadData.find(thread_id);
  if (it == m_ThreadData.end())
    return std::nullopt;

  return it->second;
}

std::optional<std::vector<std::shared_ptr<UserFriendRequest>>>
UserAccountManager::getFriendRequestList(std::size_t &begin,
                                         const std::size_t interval) {

  std::lock_guard<std::mutex> _lckg(m_mtx);
  /* user requested number even larger then greatest amount */
  if (begin >= m_friend_request_list.size()) {
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
  std::lock_guard<std::mutex> _lckg(m_mtx);
  std::vector<std::shared_ptr<UserNameCard>> list;
  std::transform(m_auth_friend_list.begin(), m_auth_friend_list.end(),
                 std::back_inserter(list),
                 [](const auto &T) { return T.second; });
  return list;
}

std::optional<std::vector<std::shared_ptr<UserNameCard>>>
UserAccountManager::getAuthFriendList(std::size_t &begin,
                                      const std::size_t interval) {

  std::lock_guard<std::mutex> _lckg(m_mtx);
  /* user requested number even larger then greatest amount */
  if (begin >= m_auth_friend_list.size())

    /*return a empty container*/
    return std::nullopt;

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
    // std::advance(it_end, interval); ???? Potential bug???
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
  std::lock_guard<std::mutex> _lckg(m_mtx);

  auto it = m_auth_friend_list.find(uuid);
  if (it == m_auth_friend_list.end())
    return std::nullopt;

  return it->second;
}

std::optional<QString>
UserAccountManager::getThreadIdByUUID(const QString &uuid) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  auto it = m_friendOnThreadsLists.find(uuid);
  if (it == m_friendOnThreadsLists.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool UserAccountManager::alreadyExistInAuthList(const QString &uuid) const {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  return m_auth_friend_list.find(uuid) != m_auth_friend_list.end();
}

bool UserAccountManager::alreadyExistInRequestList(const QString &uuid) const {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  auto it =
      std::find_if(m_friend_request_list.begin(), m_friend_request_list.end(),
                   [uuid](std::shared_ptr<UserFriendRequest> item) {
                     // uuid should equal to m_uuid(from_uuid)
                     return item->sender_card.m_uuid == uuid;
                   });

  return it != m_friend_request_list.end();
}

void UserAccountManager::clear() {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_info.clear();
  m_userInfo.reset();
  m_friend_request_list.clear();
  m_auth_friend_list.clear();
  m_ThreadData.clear();
  m_threadDescLists.clear();
  m_friendOnThreadsLists.clear();
  m_allChattingSessions.clear();
  m_currSessionLoadingSeq = 0;
}

std::optional<std::shared_ptr<UserChatThread>>
UserAccountManager::_loadSession(const std::size_t pos) {

  /*DO NOT USE LOCK HERE*/
  if (pos >= m_allChattingSessions.size()) {
    return std::nullopt;
  }

  auto thread_id = m_allChattingSessions[pos];

  auto it = m_ThreadData.find(thread_id);

  // Not Found!
  if (it == m_ThreadData.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::shared_ptr<UserChatThread>>
UserAccountManager::getCurThreadSession() {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  return _loadSession(m_currSessionLoadingSeq);
}

std::optional<std::shared_ptr<UserChatThread>>
UserAccountManager::getNextThreadSession() {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  ++m_currSessionLoadingSeq;
  return _loadSession(m_currSessionLoadingSeq);
}
void UserAccountManager::set_host(const QString &_host) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_info.host = _host;
}
void UserAccountManager::set_port(const QString &_port) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_info.port = _port;
}
void UserAccountManager::set_token(const QString &_token) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_info.token = _token;
}
void UserAccountManager::set_uuid(const QString &_uuid) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_info.uuid = _uuid;
}
void UserAccountManager::setUserInfo(std::shared_ptr<UserNameCard> info) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  m_userInfo = info;
}
void UserAccountManager::setLastThreadID(const QString &id) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  if (id.toLongLong() > m_last_thread_id.toLongLong())
    m_last_thread_id = id;
}
