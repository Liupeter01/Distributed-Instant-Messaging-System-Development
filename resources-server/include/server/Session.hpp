#pragma once
#ifndef _SESSION_HPP_
#define _SESSION_HPP_
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <network/def.hpp>
#include <queue>
#include <server/MsgNode.hpp>

class AsyncServer;
class SyncLogic;

class Session : public std::enable_shared_from_this<Session> {
  friend class AsyncServer;
  friend class SyncLogic;

  using Convertor = std::function<unsigned short(unsigned short)>;
  using Recv = RecvNode<std::string, Convertor>;
  using Send = SendNode<std::string, Convertor>;
  using RecvPtr = std::unique_ptr<Recv>;
  using SendPtr = std::unique_ptr<Send>;

public:
  Session(boost::asio::io_context &_ioc, AsyncServer *my_gate);
  ~Session();

public:
  void startSession();
  void closeSession();
  void setUUID(const std::string &uuid);
  void sendMessage(ServiceType srv_type, const std::string &message);

private:
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
  std::mutex m_mtx;
  std::queue<SendPtr> m_send_queue;

  /* the length of the header
   * the max length of receiving buffer
   */
  static constexpr std::size_t HEADER_LENGTH =
      sizeof(uint16_t) + sizeof(uint16_t);
  static constexpr std::size_t MAX_LENGTH = 2048;
};

#endif
