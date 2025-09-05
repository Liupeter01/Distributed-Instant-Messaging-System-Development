#pragma once
#ifndef USERACCOUNTMANAGER_H
#define USERACCOUNTMANAGER_H

#include "singleton.hpp"
#include <ChattingThreadDef.hpp>
#include <QObject>
#include <QString>
#include <UserDef.hpp>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

struct ChattingThreadDesc;
class UserChatThread;

enum class TargetList {
  FRIENDLIST,
  REQUESTLIST,
};

class UserAccountManager : public Singleton<UserAccountManager> {
  friend class Singleton<UserAccountManager>;

public:
  struct ChattingServerInfo {
    ChattingServerInfo() : uuid(), host(), port(), token() {}
    void clear() {
      uuid.clear();
      host.clear();
      port.clear();
      token.clear();
    }
    QString uuid;
    QString host;
    QString port;
    QString token;
  };

  ~UserAccountManager();
  void set_host(const QString &_host);
  void set_port(const QString &_port);
  void set_token(const QString &_token);
  void set_uuid(const QString &_uuid);

  const QString &get_host() const { return m_info.host; }
  const QString &get_port() const { return m_info.port; }
  const QString &get_token() const { return m_info.token; }
  const QString get_uuid() const { return m_info.uuid; }

  auto getCurUserInfo() { return m_userInfo; }

  void clear();

public:
  void appendArrayToList(TargetList target, const QJsonArray &array);

  // add friendinfo to the friend request list(Owner not granted!!!)
  void addItem2List(std::shared_ptr<UserFriendRequest> info);

  /*get all list(not recommended!)*/
  const std::vector<std::shared_ptr<UserFriendRequest>> &
  getFriendRequestList() const {
    return m_friend_request_list;
  }

  // add friend usernamecard to auth friend list
  void addItem2List(std::shared_ptr<UserNameCard> info);

  // create mapping relation of uuid<->thread_id
  // and relation between thread_id <-> ChattingThreadDesc
  void addItem2List(const QString &friend_uuid,
                    std::shared_ptr<ChattingThreadDesc> info);

  // Create all above(EXCEPT friend request list)
  void addItem2List(std::shared_ptr<UserChatThread> info);

  std::optional<std::shared_ptr<UserChatThread>>
  getChattingThreadData(const QString &thread_id);

  /*get limited amount of friending request list*/
  std::optional<std::vector<std::shared_ptr<UserFriendRequest>>>
  getFriendRequestList(std::size_t &begin, const std::size_t interval);

  /*get all list(not recommended!)*/
  std::vector<std::shared_ptr<UserNameCard>> getAuthFriendList();

  std::optional<std::vector<std::shared_ptr<UserNameCard>>>
  getAuthFriendList(std::size_t &begin, const std::size_t interval);

  /*get friend's userinfo*/
  std::optional<std::shared_ptr<UserNameCard>>
  findAuthFriendsInfo(const QString &uuid);

  /*get friend's thread_id through their uuid*/
  std::optional<QString> getThreadIdByUUID(const QString &uuid);

  bool alreadyExistInAuthList(const QString &uuid) const;
  bool alreadyExistInRequestList(const QString &uuid) const;

  void setUserInfo(std::shared_ptr<UserNameCard> info);

  void setLastThreadID(const QString &id);

  std::optional<std::shared_ptr<UserChatThread>> getCurThreadSession();

  std::optional<std::shared_ptr<UserChatThread>> getNextThreadSession();

protected:
  void appendAuthFriendList(const QJsonArray &array);
  void appendFriendRequestList(const QJsonArray &array);

  [[nodiscard]]
  std::optional<std::shared_ptr<UserChatThread>>
  _loadSession(const std::size_t pos);

private:
  UserAccountManager();

private:
  // because tctnetworkconnection class runs on another thread
  // it might cause race condition!
  mutable std::mutex m_mtx;

  ChattingServerInfo m_info;

  // for pull and retrieve last thread info from server
  QString m_last_thread_id;

  /*store current user's info*/
  std::shared_ptr<UserNameCard> m_userInfo;

  /*store incoming friending requests, which are not granted by the account
   * owner!*/
  std::vector<std::shared_ptr<UserFriendRequest>> m_friend_request_list;

  /*store all authenticated friend*/
  std::unordered_map<
      /*uuid*/ QString,
      /*namecard*/ std::shared_ptr<UserNameCard>>
      m_auth_friend_list;

  /*relation between thread_id <-> ChattingThreadDesc*/
  std::unordered_map<
      /*thread_id*/ QString,
      /*ChattingThreadDesc*/ std::shared_ptr<ChattingThreadDesc>>
      m_threadDescLists;

  /*relation between user uuid <-> thread_id*/
  std::unordered_map<
      /*uuid*/ QString,
      /*thread_id*/ QString>
      m_friendOnThreadsLists;

  /*
   * store chatting history
   * relation between thread_id <-> std::shared_ptr<UserChatThread>
   */
  std::unordered_map<
      /*thread_id */ QString,
      /*thread_data*/ std::shared_ptr<UserChatThread>>
      m_ThreadData;

  /*
   * store a sequence of QString(thread_id), for storing chatting sessions
   * we could find ChattingThreadDesc by using thread_id,
   * then we could load chat history data!
   */
  std::vector</*thread_id*/ QString> m_allChattingSessions;
  std::size_t m_currSessionLoadingSeq = 0;
};

Q_DECLARE_METATYPE(UserAccountManager::ChattingServerInfo)

#endif // USERACCOUNTMANAGER_H
