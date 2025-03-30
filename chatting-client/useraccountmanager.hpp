#ifndef USERACCOUNTMANAGER_H
#define USERACCOUNTMANAGER_H

#include "singleton.hpp"
#include <QString>
#include <UserFriendRequest.hpp>
#include <unordered_map>
#include <vector>

class QJsonArray;
struct FriendChattingHistory;

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

public:
  void appendArrayToList(TargetList target, const QJsonArray &array);

  void addItem2List(std::shared_ptr<UserFriendRequest> info);
  void addItem2List(std::shared_ptr<UserNameCard> info);
  void addItem2List(const QString &friend_uuid,
                    std::shared_ptr<FriendChattingHistory> info);

  std::optional<std::shared_ptr<FriendChattingHistory>>
  getChattingHistoryFromList(const QString &friend_uuid);

  /*get all list(not recommended!)*/
  std::vector<std::shared_ptr<UserFriendRequest>> getFriendRequestList();

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

  bool alreadyExistInAuthList(const QString &uuid) const;
  bool alreadyExistInRequestList(const QString &uuid) const;
  bool alreadyExistInHistoryList(const QString &friend_uuid) const;

  void setUserInfo(std::shared_ptr<UserNameCard> info);

protected:
  void appendAuthFriendList(const QJsonArray &array);
  void appendFriendRequestList(const QJsonArray &array);

private:
  UserAccountManager();

private:
  struct ChattingServerInfo {
    ChattingServerInfo()
          :uuid(), host(), port(), token()
    {}
    QString uuid;
    QString host;
    QString port;
    QString token;
  } m_info;

  /*store current user's info*/
  std::shared_ptr<UserNameCard> m_userInfo;

  /*store friending requests*/
  std::vector<std::shared_ptr<UserFriendRequest>> m_friend_request_list;

  /*store authenticated friend*/
  std::unordered_map<QString, std::shared_ptr<UserNameCard>> m_auth_friend_list;

  /*store chatting history*/
  std::unordered_map<QString, std::shared_ptr<FriendChattingHistory>>
      m_user_chatting_histroy;
};

#endif // USERACCOUNTMANAGER_H
