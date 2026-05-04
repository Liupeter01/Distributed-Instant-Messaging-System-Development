#include <echo_server.h>

EchoServer::EchoServer(io_context& ioc, uint16_t port)
          : acc_(ioc, tcp::endpoint(tcp::v4(), port)) {
          std::cout << "Echo server on port " << port << "\n";
          do_accept();
}

void EchoServer::do_accept() {
                    acc_.async_accept([this](auto ec, tcp::socket sock) {
                              if (!ec) {
                                        std::make_shared<EchoSession>(std::move(sock))->start();
                              }
                              do_accept();
                              });
}