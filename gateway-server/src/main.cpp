#include <config/ServerConfig.hpp>
#include <grpc/UserServicePool.hpp>
#include <grpc/VerificationServicePool.hpp>
#include <iostream>
#include <redis/RedisManager.hpp>
#include <server/GateServer.hpp>
#include <service/IOServicePool.hpp>
#include <sql/MySQLConnectionPool.hpp>

int main() {
  try {
    /*init all kinds of pools in advance
     * 1. IOServicePool
     * 2. MySQLConnectionPool
     * 3. RedisConnectionPool
     * 4. VerificationServicePool
     * 5. UserServicePool
     * */
    [[maybe_unused]] auto &service_pool = IOServicePool::get_instance();
    [[maybe_unused]] auto &sql = mysql::MySQLConnectionPool::get_instance();
    [[maybe_unused]] auto &redis = redis::RedisConnectionPool::get_instance();
    [[maybe_unused]] auto &verification =
        stubpool::VerificationServicePool::get_instance();
    [[maybe_unused]] auto &balance = stubpool::UserServicePool::get_instance();

    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait(
        [&ioc, &service_pool](boost::system::error_code ec, int sig_number) {
          if (ec) {
            return;
          }
          service_pool->shutdown();
          ioc.stop();
        });

    std::shared_ptr<GateServer> server = std::make_shared<GateServer>(
        IOServicePool::get_instance()->getIOServiceContext(), // get ioc
        ServerConfig::get_instance()->GateServerPort // get port configuration
    );
    server->serverStart();
    ioc.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
  return 0;
}
