#pragma once
#include <echo_session.h>
#include <service/IOServicePool.hpp>

class EchoServer {
  tcp::acceptor acc_;

  boost::asio::io_context &m_ioc;

public:
  EchoServer(io_context &ioc, uint16_t port);
  void startAccept();

private:
  void handleAccept(std::shared_ptr<EchoSession> session,
                    boost::system::error_code ec);
};
