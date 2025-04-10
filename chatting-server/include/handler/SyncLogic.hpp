#pragma once
#ifndef _SYNCLOGIC_HPP_
#define _SYNCLOGIC_HPP_
#include <atomic>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <buffer/ByteOrderConverter.hpp>
#include <buffer/MsgNode.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <network/def.hpp>
#include <optional>
#include <queue>
#include <redis/RedisManager.hpp>
#include <server/Session.hpp>
#include <service/ConnectionPool.hpp>
#include <singleton/singleton.hpp>
#include <sql/MySQLConnectionPool.hpp>
#include <thread>
#include <unordered_map>
#include <vector>

/*declaration*/
struct UserNameCard;
struct UserFriendRequest;

class SyncLogic : public Singleton<SyncLogic> {
  friend class Singleton<SyncLogic>;

  using RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool,
                                               redis::RedisContext>;
  using MySQLRAII = connection::ConnectionRAII<mysql::MySQLConnectionPool,
                                               mysql::MySQLConnection>;

public:
  using SessionPtr = std::shared_ptr<Session>;
  using NodePtr = std::unique_ptr<RecvNode<std::string, ByteOrderConverter>>;
  using pair = std::pair<SessionPtr, NodePtr>;
  using CallbackFunc =
      std::function<void(ServiceType, std::shared_ptr<Session>, NodePtr)>;

public:
  ~SyncLogic();
  void commit(pair recv_node);

public:
  static void generateErrorMessage(const std::string &log, ServiceType type,
                                   ServiceStatus status, SessionPtr conn);

  /*
   * get user's basic info(name, age, sex, ...) from redis
   * 1. we are going to search for info inside redis first, if nothing found,
   * then goto 2
   * 2. searching for user info inside mysql
   */
  static std::optional<std::unique_ptr<UserNameCard>>
  getUserBasicInfo(const std::string &key);

private:
  SyncLogic();

  /*SyncLogic Class Operations*/
  void shutdown();
  void processing();
  void registerCallbacks();
  void execute(pair &&node);

  /*client enter current server*/
  void incrementConnection();
  void decrementConnection();

  /*store this user belonged server into redis*/
  bool tagCurrentUser(const std::string &uuid);

  /*delete user belonged server in redis*/
  bool untagCurrentUser(const std::string &uuid);

  /*parse Json*/
  bool parseJson(std::shared_ptr<Session> session, NodePtr &recv,
                 boost::json::object &src_obj);

  /*Execute Operations*/
  void handlingLogin(ServiceType srv_type, std::shared_ptr<Session> session,
                     NodePtr recv);
  void handlingLogout(ServiceType srv_type, std::shared_ptr<Session> session,
                      NodePtr recv);

  void handlingUserSearch(ServiceType srv_type,
                          std::shared_ptr<Session> session, NodePtr recv);

  /*the person who init friend request*/
  void handlingFriendRequestCreator(ServiceType srv_type,
                                    std::shared_ptr<Session> session,
                                    NodePtr recv);

  /*the person who receive friend request are going to confirm it*/
  void handlingFriendRequestConfirm(ServiceType srv_type,
                                    std::shared_ptr<Session> session,
                                    NodePtr recv);

  /*Handling the user send chatting text msg to others*/
  void handlingTextChatMsg(ServiceType srv_type,
                           std::shared_ptr<Session> session, NodePtr recv);

  /*Handling the user send chatting voice msg to others*/
  void handlingVoiceChatMsg(ServiceType srv_type,
                            std::shared_ptr<Session> session, NodePtr recv);

  /*Handling the user send chatting video msg to others*/
  void handlingVideoChatMsg(ServiceType srv_type,
                            std::shared_ptr<Session> session, NodePtr recv);

  /*
   * get friend request list from the database
   * @param: startpos: get friend request from the index[startpos]
   * @param: interval: how many requests are going to acquire [startpos,
   * startpos + interval)
   */
  std::optional<std::vector<std::unique_ptr<UserFriendRequest>>>
  getFriendRequestInfo(const std::string &dst_uuid,
                       const std::size_t start_pos = 0,
                       const std::size_t interval = 10);

  /*
   * acquire Friend List
   * get existing authenticated bid-directional friend from database
   * @param: startpos: get friend from the index[startpos]
   * @param: interval: how many friends re going to acquire [startpos, startpos
   * + interval)
   */
  std::optional<std::vector<std::unique_ptr<UserNameCard>>>
  getAuthFriendsInfo(const std::string &dst_uuid,
                     const std::size_t start_pos = 0,
                     const std::size_t interval = 10);

public:
  /*redis*/
  static std::string redis_server_login;

  /*store user base info in redis*/
  static std::string user_prefix;

  /*store the server name that this user belongs to*/
  static std::string server_prefix;

private:
  std::atomic<bool> m_stop;

  /*working thread, handling commited request*/
  std::thread m_working;

  /*mutex & cv => thread safety*/
  std::mutex m_mtx;
  std::condition_variable m_cv;

  /*user commit data to the queue*/
  std::queue<pair> m_queue;

  /*callback list*/
  std::unordered_map<ServiceType, CallbackFunc> m_callbacks;
};

#endif //_SYNCLOGIC_HPP_
