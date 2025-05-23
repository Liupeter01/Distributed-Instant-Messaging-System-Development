#pragma once
#ifndef _SESSION_HPP_
#define _SESSION_HPP_
#include <boost/asio.hpp>
#include <buffer/ByteOrderConverter.hpp>
#include <buffer/MsgNode.hpp>
#include <memory>
#include <network/def.hpp>
#include <redis/RedisManager.hpp>
#include <service/ConnectionPool.hpp>
#include <tbb/concurrent_queue.h>

namespace grpc {
class GrpcDistributedChattingImpl;
}

class AsyncServer;
class SyncLogic;
class UserManager;

class Session : public std::enable_shared_from_this<Session> {
  friend class AsyncServer;
  friend class SyncLogic;
  friend class UserManager;
  friend class grpc::GrpcDistributedChattingImpl;

  using Recv = RecvNode<std::string, ByteOrderConverter>;
  using Send = SendNode<std::string, ByteOrderConverterReverse>;
  using RecvPtr = std::unique_ptr<Recv>;
  using SendPtr = std::unique_ptr<Send>;

  using RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool,
                                               redis::RedisContext>;

public:
  Session(boost::asio::io_context &_ioc, AsyncServer *my_gate);
  ~Session();

public:
  void startSession();
  void closeSession();
  void sendOfflineMessage();
  void setUUID(const std::string &uuid) { s_uuid = uuid; }
  void sendMessage(ServiceType srv_type, const std::string &message,
                   std::shared_ptr<Session> self);
  [[nodiscard]] bool isSessionTimeout(const std::time_t &now) const;
  void updateLastHeartBeat();
  const std::string &get_user_uuid() const;
  const std::string &get_session_id() const;

  void markAsDeferredTerminated(std::function<void()> &&callable);

protected:
  /*handling sending event*/
  void handle_write(std::shared_ptr<Session> session,
                    boost::system::error_code ec);

  /*handling receive event!*/
  void handle_header(std::shared_ptr<Session> session,
                     boost::system::error_code ec,
                     std::size_t bytes_transferred);
  void handle_msgbody(std::shared_ptr<Session> session,
                      boost::system::error_code ec,
                      std::size_t bytes_transferred);

  bool checkDeferredTermination();

private:
  /*
   *  sub user connection counter for current server
   * 1. HGET not exist: Current Chatting server didn't setting up connection
   * counter
   * 2. HGET exist: Decrement by 1
   */
  void decrementConnection();

  void terminateAndRemoveFromServer(const std::string &user_uuid);
  void terminateAndRemoveFromServer(const std::string &user_uuid,
                                    const std::string &expected_session_id);
  static void removeRedisCache(const std::string &uuid,
                               const std::string &session_id);

  void purgeRemoveConnection(std::shared_ptr<Session> session);

private:
  /*redis*/
  static std::string redis_server_login;

  /*store the server name that this user belongs to*/
  static std::string server_prefix;

  /*store the current session id that this user belongs to*/
  static std::string session_prefix;

  bool s_closed = false;

  enum class SessionState : uint8_t {
    Unkown,
    Alive, // online
    Kicked,
    LogoutPending, //
    Terminated
  };

  /*Session State flag*/
  std::atomic<SessionState> m_state = Session::SessionState::Alive;

  /*logout pending handler*/
  std::function<void()> m_finalSendCompleteHandler;

  /*store unique user id(uuid)*/
  std::string s_uuid;

  /*store unique session id*/
  std::string s_session_id;

  /*user's socket*/
  boost::asio::ip::tcp::socket s_socket;

  /*
   * the last time that this session recv data from client
   * we can not consider about boost::asio::steady_timer, because it's unsafe
   */
  std::atomic<std::time_t> m_last_heartbeat;

  /*pointing to the server it belongs to*/
  AsyncServer *s_gate;

  /*header and message recv buffer, after receiving header, m_header_status flag
   * will be set to true*/
  RecvPtr m_recv_buffer;

  /*sending queue*/
  std::atomic<bool> m_write_in_progress = false;
  std::unique_ptr<Send> m_current_write_msg;
  tbb::concurrent_queue<SendPtr> m_concurrent_sent_queue;

  /* the length of the header
   * the max length of receiving buffer
   */
  static constexpr std::size_t MAX_LENGTH = 2048;
};

#endif
