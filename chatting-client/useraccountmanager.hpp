#pragma once
#ifndef USERACCOUNTMANAGER_H
#define USERACCOUNTMANAGER_H

#include "singleton.hpp"
#include <ChattingThreadDef.hpp>
#include <QString>
#include <UserDef.hpp>
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
  ~UserAccountManager();
  void set_host(const QString &_host) { m_info.host = _host; }
  void set_port(const QString &_port) { m_info.port = _port; }
  void set_token(const QString &_token) { m_info.token = _token; }
  void set_uuid(const QString &_uuid) { m_info.uuid = _uuid; }

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
  void addItem2List(const QString &friend_uuid,
                    std::shared_ptr<ChattingThreadDesc> info);

  void addItem2List(const QString &thread_id,
                    std::shared_ptr<UserChatThread> info);

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

  void setUserInfo(std::shared_ptr<UserNameCard> info) { m_userInfo = info; }

  void setLastThreadID(const QString &id) {
    if (id.toLongLong() > m_last_thread_id.toLongLong()) {
      m_last_thread_id = id;
    }
    // m_last_thread_id = id;
  }

protected:
  void appendAuthFriendList(const QJsonArray &array);
  void appendFriendRequestList(const QJsonArray &array);

private:
  UserAccountManager();

private:
  // for pull and retrieve last thread info from server
  QString m_last_thread_id;

  struct ChattingServerInfo {
    ChattingServerInfo();
    void clear();
    QString uuid;
    QString host;
    QString port;
    QString token;
  } m_info;

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
      /*thread_id list*/ // std::vector<QString>
      /*thread_id*/ QString>
      m_friendOnThreadsLists;

  /*
   * store chatting history
   * relation between thread_id <-> std::shared_ptr<UserChatThread>
   */
  std::unordered_map<
      /*thread_id or temp_id */ QString,
      /*thread_data*/ std::shared_ptr<UserChatThread>>
      m_ThreadData;
};

#endif // USERACCOUNTMANAGER_H
