#include <config/ServerConfig.hpp>
#include <grpc/GrpcResourcesService.hpp>
#include <grpc/ResourcesServicePool.hpp>
#include <handler/SyncLogic.hpp>
#include <redis/RedisManager.hpp>
#include <server/AsyncServer.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/spdlog.h>
#include <sql/MySQLConnectionPool.hpp>

// redis_server_login hash
static std::string redis_server_login = "redis_server";

int main() {
  try {
    [[maybe_unused]] auto &service_pool = IOServicePool::get_instance();
    [[maybe_unused]] auto &mysql = mysql::MySQLConnectionPool::get_instance();
    [[maybe_unused]] auto &redis = redis::RedisConnectionPool::get_instance();
    [[maybe_unsued]] auto &resources_pool =
        stubpool::ResourcesServicePool::get_instance();

    /*chatting server port and grpc server port should not be same!!*/
    if (ServerConfig::get_instance()->GrpcServerPort ==
        ServerConfig::get_instance()->ResourceServerPort) {
      spdlog::error("[Resources Service {}]: Resources Server's Port Should Be "
                    "Different Comparing to GRPC Server!",
                    ServerConfig::get_instance()->GrpcServerName);
      std::abort();
    }

    ///*gRPC server*/
    // std::string address =
    //           fmt::format("{}:{}",
    //           ServerConfig::get_instance()->GrpcServerHost,
    //                     ServerConfig::get_instance()->GrpcServerPort);

    // spdlog::info("Current Resources RPC Server Started Running On {}",
    // address);

    auto response = gRPCResourcesService::registerResourcesServerInstance(
        ServerConfig::get_instance()->GrpcServerName,
        ServerConfig::get_instance()->GrpcServerHost,
        std::to_string(ServerConfig::get_instance()->ResourceServerPort));

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::critical("[Resources Service {}] Balance-Server Not Available! "
                       "Try register Resources Server Instance Failed!, "
                       "error code {}",
                       ServerConfig::get_instance()->GrpcServerName,
                       response.error());
      std::abort();
    }

    spdlog::info(
        "[Resources Server] Register Resources Server Instance Successful");

    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait(
        [&ioc, &service_pool](boost::system::error_code ec, int sig_number) {
          if (ec) {
            return;
          }
          spdlog::critical("Resources Server exit due to control-c input!");
          ioc.stop();
          service_pool->shutdown();
        });

    /*set current server connection counter value(0) to hash by using HSET*/
    connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
        raii;
    raii->get()->setValue2Hash(redis_server_login,
                               ServerConfig::get_instance()->GrpcServerName,
                               std::to_string(0));

    /*create register server*/
    std::shared_ptr<AsyncServer> async = std::make_shared<AsyncServer>(
        service_pool->getIOServiceContext(),
        ServerConfig::get_instance()->ResourceServerPort);

    async->startAccept();
    /**/
    ioc.run();

    /*
     * Resources  server shutdown
     * Delete current server connection counter by using HDEL
     */
    raii->get()->delValueFromHash(redis_server_login,
                                  ServerConfig::get_instance()->GrpcServerName);

    /*
     * Resources Server Shutdown
     * Delete current server
     */
    response = gRPCResourcesService::resourcesServerShutdown(
        ServerConfig::get_instance()->GrpcServerName);

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[{}] Try Remove Current Resources Server Instance From "
                    "Lists Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
    }

    spdlog::info(
        "[Resources Server] Unregister Resources Server Instance Successful");

  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
  return 0;
}
