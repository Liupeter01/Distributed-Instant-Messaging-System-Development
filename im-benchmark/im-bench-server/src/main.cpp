#include <echo_server.h>
#include <service/IOServicePool.hpp>

int main(int argc, char **argv) {

  auto &service_pool = IOServicePool::get_instance();
  uint16_t port = 8888;

  boost::asio::io_context ioc;
  boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
  signal.async_wait(
      [&ioc, &service_pool](boost::system::error_code ec, int sig_number) {
        if (ec) {
          return;
        }
        ioc.stop();
        service_pool->shutdown();
      });

  EchoServer srv(service_pool->getIOServiceContext(), port);
  srv.startAccept();
  ioc.run();
}
