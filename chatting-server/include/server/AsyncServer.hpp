#pragma once
#ifndef _ASYNCSERVER_HPP_
#define _ASYNCSERVER_HPP_
#include <server/Session.hpp>

class SyncLogic;

class AsyncServer : public std::enable_shared_from_this<AsyncServer> {
  friend class Session;
  friend class SyncLogic;

public:
  AsyncServer(boost::asio::io_context &_ioc, unsigned short port);
  ~AsyncServer();

public:
  void startAccept();

protected:
  // waiting to be closed
  void moveUserToTerminationZone(const std::string &user_uuid);
  void terminateConnection(const std::string &user_uuid);
  void terminateConnection(const std::string &user_uuid,
                           const std::string &expected_session_id);
  void handleAccept(std::shared_ptr<Session> session,
                    boost::system::error_code ec);

private:
  void registerTimerCallback();
  void heartBeatEvent(const boost::system::error_code &ec);

private:
  /*boost io_context*/
  boost::asio::io_context &m_ioc;

  /*create a server acceptor to accept connection*/
  boost::asio::ip::tcp::acceptor m_acceptor;

  /*timer & clock*/
  boost::asio::steady_timer m_timer;
};

#endif
