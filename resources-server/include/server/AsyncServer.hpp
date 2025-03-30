#pragma once
#ifndef _ASYNCSERVER_HPP_
#define _ASYNCSERVER_HPP_
#include <mutex>
#include <server/Session.hpp>
#include <unordered_map>

class SyncLogic;

class AsyncServer : public std::enable_shared_from_this<AsyncServer> {
  friend class Session;
  friend class SyncLogic;

public:
  AsyncServer(boost::asio::io_context &_ioc, unsigned short port);
  ~AsyncServer();

public:
  void startAccept();

private:
  void terminateConnection(const std::string &session_id);
  void handleAccept(std::shared_ptr<Session> session,
                    boost::system::error_code ec);

private:
  /*create a mutex to protect m_sessions*/
  std::mutex m_mtx;

  /*boost io_context*/
  boost::asio::io_context &m_ioc;

  /*create a server acceptor to accept connection*/
  boost::asio::ip::tcp::acceptor m_acceptor;

  /*maintain user's connection*/
  std::unordered_map<
      /*unique_string*/ std::string,
      /*connected session*/ std::shared_ptr<Session>>
      m_sessions;
};

#endif
