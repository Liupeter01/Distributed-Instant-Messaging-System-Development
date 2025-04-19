#pragma once
#ifndef _SESSION_HPP_
#define _SESSION_HPP_
#include <memory>
#include <boost/asio.hpp>
#include <network/def.hpp>
#include <buffer/MsgNode.hpp>
#include <tbb/concurrent_queue.h>
#include <redis/RedisManager.hpp>
#include <service/ConnectionPool.hpp>
#include <buffer/ByteOrderConverter.hpp>

class AsyncServer;
class SyncLogic;

class Session : public std::enable_shared_from_this<Session> {
  friend class AsyncServer;
  friend class SyncLogic;

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
  void sendMessage(ServiceType srv_type, const std::string &message);
  const std::string &get_user_uuid() const;
  const std::string &get_session_id() const;

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

private:
  void terminateAndRemoveFromServer(const std::string &user_uuid);
  void terminateAndRemoveFromServer(const std::string& user_uuid, const std::string& expected_session_id);
  static void removeRedisCache(const std::string& uuid, const std::string& session_id);

private:
          /*store the server name that this user belongs to*/
          static std::string server_prefix;

          /*store the current session id that this user belongs to*/
          static std::string session_prefix;

  bool s_closed = false;

  /*store unique user id(uuid)*/
  std::string s_uuid;

  /*store unique session id*/
  std::string s_session_id;

  /*user's socket*/
  boost::asio::ip::tcp::socket s_socket;

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
