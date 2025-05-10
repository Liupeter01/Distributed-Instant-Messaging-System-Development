#include <config/ServerConfig.hpp>
#include <grpc/DistributedChattingServicePool.hpp>
#include <grpc/GrpcDistributedChattingImpl.hpp>
#include <grpc/GrpcRegisterChattingService.hpp>
#include <grpc/RegisterChattingServicePool.hpp>
#include <grpc/UserServicePool.hpp>
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
    [[maybe_unused]] auto &user = stubpool::UserServicePool::get_instance();
    [[maybe_unused]] auto &chatting =
        stubpool::RegisterChattingServicePool::get_instance();
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

    spdlog::info("[{}]: RPC Server Started Running On {}",
                 ServerConfig::get_instance()->GrpcServerName, address);

    /*Distributed Chatting Server Impl*/
    grpc::ServerBuilder builder;
    grpc::GrpcDistributedChattingImpl impl;

    /*binding ports and establish service*/
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&impl);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    /*execute grpc server in another thread*/
    std::thread grpc_server_thread([&server]() { server->Wait(); });

    auto response =
        gRPCGrpcRegisterChattingService::registerChattingServerInstance(
            ServerConfig::get_instance()->GrpcServerName,
            ServerConfig::get_instance()->GrpcServerHost,
            std::to_string(ServerConfig::get_instance()->ChattingServerPort));

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::critical("[{}] Balance-Server Not Available! "
                       "Try register Chatting Server Instance Failed!, "
                       "error code {}",
                       ServerConfig::get_instance()->GrpcServerName,
                       response.error());
      std::abort();
    }

    spdlog::info("[{}] Register Chatting Server Instance Successful",
                 ServerConfig::get_instance()->GrpcServerName);

    /*register grpc server to balance-server lists*/
    response = gRPCGrpcRegisterChattingService::registerGrpcServer(
        ServerConfig::get_instance()->GrpcServerName,
        ServerConfig::get_instance()->GrpcServerHost,
        std::to_string(ServerConfig::get_instance()->GrpcServerPort));

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[{}] Balance-Server Not Available! Try "
                    "register GRPC Server Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
      std::abort();
    }

    spdlog::info("[{}] Register Chatting Grpc Server Successful",
                 ServerConfig::get_instance()->GrpcServerName);

    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait([&ioc, &service_pool,
                       &server](boost::system::error_code ec, int sig_number) {
      if (ec) {
        return;
      }
      spdlog::critical("[{}] Exit Due To Control-c Input!",
                       ServerConfig::get_instance()->GrpcServerName);
      ioc.stop();
      service_pool->shutdown();
      server->Shutdown();
    });

    /*set current server connection counter value(0) to hash by using HSET*/
    connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
        raii;
    auto get_distributed_lock =
        raii->get()->acquire(ServerConfig::get_instance()->GrpcServerName, 10,
                             10, redis::TimeUnit::Milliseconds);

    if (!get_distributed_lock.has_value()) {
      spdlog::error("[{}] Acquire Distributed-Lock In Startup Phase Failed!",
                    ServerConfig::get_instance()->GrpcServerName);
      return 0;
    }
    spdlog::info("[{}] Acquire Distributed-Lock In Startup Phase Successful!",
                 ServerConfig::get_instance()->GrpcServerName);

    if (!raii->get()->setValue2Hash(
            redis_server_login, ServerConfig::get_instance()->GrpcServerName,
            std::to_string(0))) {

      spdlog::error("[{}] Client Number Can Not Be Written To Redis Cache "
                    "During Startup Phase! Error Occured!",
                    ServerConfig::get_instance()->GrpcServerName);
    } else {
      spdlog::info(
          "[{}] Now Current Server Init During Startup Phase Successful",
          ServerConfig::get_instance()->GrpcServerName);
    }

    // release lock
    raii->get()->release(ServerConfig::get_instance()->GrpcServerName,
                         get_distributed_lock.value());

    /*create chatting server*/
    std::shared_ptr<AsyncServer> async = std::make_shared<AsyncServer>(
        service_pool->getIOServiceContext(),
        ServerConfig::get_instance()->ChattingServerPort);

    async->startAccept();
    async->startTimer();      //start zombie kill timer

    /**/
    ioc.run();

    /*join subthread*/
    if (grpc_server_thread.joinable()) {
      grpc_server_thread.join();
    }

    async->stopTimer();       //terminate timer!
    
    /*
     * Chatting server shutdown
     * Delete current chatting server connection counter by using HDEL
     */
    get_distributed_lock =
        raii->get()->acquire(ServerConfig::get_instance()->GrpcServerName, 10,
                             10, redis::TimeUnit::Milliseconds);

    if (!get_distributed_lock.has_value()) {
      spdlog::error("[{}] Acquire Distributed-Lock In Shutdown Phase Failed!",
                    ServerConfig::get_instance()->GrpcServerName);
      return 0;
    }

    if (!raii->get()->delValueFromHash(
            redis_server_login, ServerConfig::get_instance()->GrpcServerName)) {

      spdlog::error("[{}] Client Number Can Not Be Written To Redis Cache "
                    "During Shutdown Phase! Error Occured!",
                    ServerConfig::get_instance()->GrpcServerName);
    } else {
      spdlog::info(
          "[{}] Now Current Server Init During Shutdown Phase Successful",
          ServerConfig::get_instance()->GrpcServerName);
    }

    // release lock
    raii->get()->release(ServerConfig::get_instance()->GrpcServerName,
                         get_distributed_lock.value());

    /*
     * Chatting Server Shutdown
     * Delete current chatting server
     */
    response = gRPCGrpcRegisterChattingService::chattingServerShutdown(
        ServerConfig::get_instance()->GrpcServerName);

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[{}] Try Remove Current Chatting Server Instance From "
                    "Lists Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
    }

    spdlog::info("[{}] Remove Chatting Server Instance From Balance Successful",
                 ServerConfig::get_instance()->GrpcServerName);

    /*
     * grpc Server Shutdown
     * Delete current grpc server from balance-server grpc lists
     */
    response = gRPCGrpcRegisterChattingService::grpcServerShutdown(
        ServerConfig::get_instance()->GrpcServerName);

    if (response.error() !=
        static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::error("[{}] Try Remove Current GRPC Server From Lists Failed!, "
                    "error code {}",
                    ServerConfig::get_instance()->GrpcServerName,
                    response.error());
    }

    spdlog::info("[{}] Remove Chatting Grpc Server From Balance Successful",
                 ServerConfig::get_instance()->GrpcServerName);

  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
  return 0;
}
