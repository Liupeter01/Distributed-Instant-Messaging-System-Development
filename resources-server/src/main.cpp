#include <config/ServerConfig.hpp>
#include <handler/SyncLogic.hpp>
#include <server/AsyncServer.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/spdlog.h>

// redis_server_login hash
static std::string redis_server_login = "redis_server";

int main() {
  try {
    [[maybe_unused]] auto &service_pool = IOServicePool::get_instance();

    /*chatting server port and grpc server port should not be same!!*/
    if (ServerConfig::get_instance()->GrpcServerPort ==
        ServerConfig::get_instance()->ResourceServerPort) {
      spdlog::error("[Resources Service {}]: Resources Server's Port Should Be "
                    "Different Comparing to GRPC Server!",
                    ServerConfig::get_instance()->GrpcServerName);
      std::abort();
    }

    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait([&ioc, &service_pool](boost::system::error_code ec, int sig_number) {
      if (ec) {
        return;
      }
      spdlog::critical("Resources Server exit due to control-c input!");
      ioc.stop();
      service_pool->shutdown();
    });

    /*create chatting server*/
    std::shared_ptr<AsyncServer> async = std::make_shared<AsyncServer>(
        service_pool->getIOServiceContext(),
        ServerConfig::get_instance()->ResourceServerPort);

    async->startAccept();
    /**/
    ioc.run();

  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
  return 0;
}
