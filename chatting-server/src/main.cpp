#include <config/ServerConfig.hpp>
#include <grpc/BalanceServicePool.hpp>
#include <grpc/DistributedChattingServicePool.hpp>
#include <grpc/GrpcBalanceService.hpp>
#include <grpc/GrpcDistributedChattingImpl.hpp>
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
    [[maybe_unused]] auto &balance =
        stubpool::BalancerServicePool::get_instance();
    [[maybe_unused]] auto &distribute =
        stubpool::DistributedChattingServicePool::get_instance();

    /*chatting server port and grpc server port should not be same!!*/
    if (ServerConfig::get_instance()->GrpcServerPort ==
        ServerConfig::get_instance()->ChattingServerPort) {
      spdlog::error("[Chatting Service {}] :Chatting Server's Port Should Be "
                    "Different Comparing to GRPC Server!",
                    ServerConfig::get_instance()->GrpcServerName);
      std::abort();
    }

    /*gRPC server*/
    std::string address =
        fmt::format("{}:{}", ServerConfig::get_instance()->GrpcServerHost,
                    ServerConfig::get_instance()->GrpcServerPort);

    spdlog::info("[Chatting Server]: RPC Server Started Running On {}",
                 address);

    /*Distributed Chatting Server Impl*/
    grpc::ServerBuilder builder;
    grpc::GrpcDistributedChattingImpl impl;

    /*binding ports and establish service*/
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&impl);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    /*execute grpc server in another thread*/
    std::thread grpc_server_thread([&server]() { server->Wait(); });

    auto response = gRPCBalancerService::registerChattingServerInstance(
        ServerConfig::get_instance()->GrpcServerName,
        ServerConfig::get_instance()->GrpcServerHost,
        std::to_string(ServerConfig::get_instance()->ChattingServerPort));

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::critical("[Chatting Service {}] Balance-Server Not Available! "
                       "Try register Chatting Server Instance Failed!, "
                       "error code {}",
                       ServerConfig::get_instance()->GrpcServerName,
                       response.error());
      std::abort();
    }

    spdlog::info(
        "[Chatting Server] Register Chatting Server Instance Successful");

    /*register grpc server to balance-server lists*/
    response = gRPCBalancerService::registerGrpcServer(
        ServerConfig::get_instance()->GrpcServerName,
        ServerConfig::get_instance()->GrpcServerHost,
        std::to_string(ServerConfig::get_instance()->GrpcServerPort));

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[Chatting Service {}] Balance-Server Not Available! Try "
                    "register GRPC Server Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
      std::abort();
    }

    spdlog::info("[Chatting Service] Register Chatting Grpc Server Successful");

    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait([&ioc, &service_pool,
                       &server](boost::system::error_code ec, int sig_number) {
      if (ec) {
        return;
      }
      spdlog::critical(
          "[Chatting Service] chatting server exit due to control-c input!");
      ioc.stop();
      service_pool->shutdown();
      server->Shutdown();
    });

    /*set current server connection counter value(0) to hash by using HSET*/
    connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
        raii;
    raii->get()->setValue2Hash(redis_server_login,
                               ServerConfig::get_instance()->GrpcServerName,
                               std::to_string(0));

    /*create chatting server*/
    std::shared_ptr<AsyncServer> async = std::make_shared<AsyncServer>(
        service_pool->getIOServiceContext(),
        ServerConfig::get_instance()->ChattingServerPort);

    async->startAccept();
    /**/
    ioc.run();

    /*join subthread*/
    if (grpc_server_thread.joinable()) {
      grpc_server_thread.join();
    }

    /*
     * Chatting server shutdown
     * Delete current chatting server connection counter by using HDEL
     */
    raii->get()->delValueFromHash(redis_server_login,
                                  ServerConfig::get_instance()->GrpcServerName);

    /*
     * Chatting Server Shutdown
     * Delete current chatting server
     */
    response = gRPCBalancerService::chattingServerShutdown(
        ServerConfig::get_instance()->GrpcServerName);

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[{}] Try Remove Current Chatting Server Instance From "
                    "Lists Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
    }

    spdlog::info(
        "[Chatting Service {}] Unregister Chatting Server Instance Successful");

    /*
     * grpc Server Shutdown
     * Delete current grpc server from balance-server grpc lists
     */
    response = gRPCBalancerService::grpcServerShutdown(
        ServerConfig::get_instance()->GrpcServerName);

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[{}] Try Remove Current GRPC Server From Lists Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
    }

    spdlog::info(
        "[Chatting Server] Unregister Chatting Grpc Server Successful");

  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
  return 0;
}
