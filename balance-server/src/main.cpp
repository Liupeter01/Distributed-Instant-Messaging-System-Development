#include <boost/asio.hpp>
#include <config/ServerConfig.hpp>
#include <grpc/GrpcUserServiceImpl.hpp>
#include <grpc/GrpcChattingImpl.hpp>
#include <grpc/GrpcResourcesImpl.hpp>
#include <redis/RedisManager.hpp>
#include <thread>

int main() {
  [[maybe_unused]] auto &redis = redis::RedisConnectionPool::get_instance();

  std::string address =
      fmt::format("{}:{}", ServerConfig::get_instance()->BalanceServiceAddress,
                  ServerConfig::get_instance()->BalanceServicePort);

  spdlog::info("[Balance Server]: RPC Server Started Running On [{}]", address);

  /*gRPC server*/
  grpc::ServerBuilder builder;
  grpc::GrpcUserServiceImpl user_impl;
  grpc::GrpcChattingImpl chat_impl;
  grpc::GrpcResourcesImpl resource_impl;

  /*binding ports and establish service*/
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&user_impl);
  builder.RegisterService(&chat_impl);
  builder.RegisterService(&resource_impl);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  /*execute grpc server in another thread*/
  std::thread grpc_server_thread([&server]() { server->Wait(); });

  try {
    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait(
        [&ioc, &server](boost::system::error_code ec, int sig_number) {
          if (ec) {
            return;
          }
          spdlog::critical("balance server exit due to control-c input!");
          ioc.stop();
          server->Shutdown();
        });

    /**/
    ioc.run();

    /*join subthread*/
    if (grpc_server_thread.joinable()) {
      grpc_server_thread.join();
    }

    server->Wait();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
  return 0;
}
