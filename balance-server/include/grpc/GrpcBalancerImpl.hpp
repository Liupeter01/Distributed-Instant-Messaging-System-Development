#pragma once
#ifndef _GRPCBALANCERIMPL_HPP_
#define _GRPCBALANCERIMPL_HPP_
#include <grpcpp/grpcpp.h>
#include <memory>
#include <message/message.grpc.pb.h>
#include <mutex>
#include <network/def.hpp>
#include <optional>
#include <unordered_map>

namespace grpc {
class GrpcBalancerImpl final : public message::BalancerService::Service {

public:
  struct ChattingServerConfig {
    ChattingServerConfig(const std::string &host, const std::string &port,
                         const std::string &name)
        : _connections(0), _host(host), _port(port), _name(name) {}
    std::string _host;
    std::string _port;
    std::string _name;
    std::size_t _connections = 0; /*add init*/
  };

  struct GRPCServerConfig {
    GRPCServerConfig(const std::string &host, const std::string &port,
                     const std::string &name)
        : _host(host), _port(port), _name(name) {}
    std::string _host;
    std::string _port;
    std::string _name;
  };

  ~GrpcBalancerImpl();
  GrpcBalancerImpl();

public:
  // pass user's uuid parameter to the server, and returns available server
  // address to user
  virtual ::grpc::Status
  AddNewUserToServer(::grpc::ServerContext *context,
                     const ::message::RegisterToBalancer *request,
                     ::message::GetAllocatedChattingServer *response);

  // user send SERVICE_LOGINSERVER request
  virtual ::grpc::Status
  UserLoginToServer(::grpc::ServerContext *context,
                    const ::message::LoginChattingServer *request,
                    ::message::LoginChattingResponse *response);

  virtual ::grpc::Status
            UserLogoutFromServer(::grpc::ServerContext* context,
                      const ::message::LogoutChattingServer* request,
                      ::message::LogoutChattingResponse* response);

  // chatting server acquires other servers info through this service
  virtual ::grpc::Status
  GetPeerChattingServerInfo(::grpc::ServerContext *context,
                            const ::message::PeerListsRequest *request,
                            ::message::PeerResponse *response);

  virtual ::grpc::Status
  GetPeerGrpcServerInfo(::grpc::ServerContext *context,
                        const ::message::PeerListsRequest *request,
                        ::message::PeerResponse *response);

  virtual ::grpc::Status
  RegisterChattingServerInstance(::grpc::ServerContext *context,
                                 const ::message::GrpcRegisterRequest *request,
                                 ::message::GrpcStatusResponse *response);

  virtual ::grpc::Status
  RegisterChattingGrpcServer(::grpc::ServerContext *context,
                             const ::message::GrpcRegisterRequest *request,
                             ::message::GrpcStatusResponse *response);

  virtual ::grpc::Status
  ChattingServerShutDown(::grpc::ServerContext *context,
                         const ::message::GrpcShutdownRequest *request,
                         ::message::GrpcStatusResponse *response);

  virtual ::grpc::Status
  ChattingGrpcServerShutDown(::grpc::ServerContext *context,
                             const ::message::GrpcShutdownRequest *request,
                             ::message::GrpcStatusResponse *response);

  static std::string userTokenGenerator();

private:
  std::optional<std::shared_ptr<grpc::GrpcBalancerImpl::ChattingServerConfig>>
  serverLoadBalancer();
  void registerUserInfo(std::size_t uuid, std::string &&tokens);

  /*get user token from Redis*/
  std::optional<std::string> getUserToken(std::size_t uuid);
  ServiceStatus verifyUserToken(std::size_t uuid, const std::string &tokens);

private:
  /*redis*/
  const std::string redis_server_login = "redis_server";

  /*user token predix*/
  const std::string token_prefix = "user_token_";

  struct UserInfo {
    UserInfo(std::string &&tokens,
             const grpc::GrpcBalancerImpl::ChattingServerConfig &config);

    /*user token*/
    std::string m_tokens;

    /*server info*/
    std::string_view m_host;
    std::string_view m_port;
  };

  std::mutex grpc_mtx;
  std::unordered_map<
      /*server name*/ std::string,
      /*server info*/ std::unique_ptr<GRPCServerConfig>>
      grpc_servers;

  std::mutex chatting_mtx;
  std::unordered_map<
      /*server name*/ std::string,
      /*server info*/ std::unique_ptr<ChattingServerConfig>>
      chatting_servers;
};
} // namespace grpc
#endif
