#include <echo_server.h>

EchoServer::EchoServer(io_context &ioc, uint16_t port)
    : acc_(ioc, tcp::endpoint(boost::asio::ip::address_v4::any(), port)),
      m_ioc(ioc) {
  std::cout << "Echo server on port " << port << "\n";

  acc_.listen(boost::asio::socket_base::max_listen_connections);
}

void EchoServer::startAccept() {
  auto &ioc = IOServicePool::get_instance()->getIOServiceContext();
  std::shared_ptr<EchoSession> session = std::make_shared<EchoSession>(ioc);

  acc_.async_accept(
      session->sock_,
      std::bind(&EchoServer::handleAccept, this, session,
                std::placeholders::_1) /*extend the life length of the session*/
  );
}

void EchoServer::handleAccept(std::shared_ptr<EchoSession> session,
                              boost::system::error_code ec) {
  if (!ec) {
    /*start session read and write function*/
    session->start();
  }
  this->startAccept();
}
