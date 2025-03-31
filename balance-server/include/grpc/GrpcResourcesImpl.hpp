#pragma once
#ifndef _GRPCRESOURCESIMPL_HPP_
#define _GRPCRESOURCESIMPL_HPP_
#include <grpcpp/grpcpp.h>
#include <memory>
#include <message/message.grpc.pb.h>
#include <mutex>
#include <network/def.hpp>
#include <optional>
#include <unordered_map>

namespace grpc {

class GrpcResourcesImpl final : public message::ResourceService::Service {
public:
  struct ResourcesServerConfig {
    ResourcesServerConfig(const std::string &host, const std::string &port,
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

  ~GrpcResourcesImpl();
  GrpcResourcesImpl();

public:
  virtual ::grpc::Status
  GetPeerResourceServerInfo(::grpc::ServerContext *context,
                            const ::message::PeerListsRequest *request,
                            ::message::PeerResponse *response);

  virtual ::grpc::Status
  GetPeerResourceGrpcServerInfo(::grpc::ServerContext *context,
                                const ::message::PeerListsRequest *request,
                                ::message::PeerResponse *response);

  virtual ::grpc::Status
  RegisterResourceServerInstance(::grpc::ServerContext *context,
                                 const ::message::GrpcRegisterRequest *request,
                                 ::message::GrpcStatusResponse *response);

  virtual ::grpc::Status
  RegisterResourceGrpcServer(::grpc::ServerContext *context,
                             const ::message::GrpcRegisterRequest *request,
                             ::message::GrpcStatusResponse *response);

  virtual ::grpc::Status
  ResourceServerShutDown(::grpc::ServerContext *context,
                         const ::message::GrpcShutdownRequest *request,
                         ::message::GrpcStatusResponse *response);

  virtual ::grpc::Status
  ResourceGrpcServerShutDown(::grpc::ServerContext *context,
                             const ::message::GrpcShutdownRequest *request,
                             ::message::GrpcStatusResponse *response);

private:
  std::optional<std::shared_ptr<ResourcesServerConfig>> serverLoadBalancer();
  // void registerUserInfo(std::size_t uuid, std::string&& tokens);

  ///*get user token from Redis*/
  // std::optional<std::string> getUserToken(std::size_t uuid);
  // ServiceStatus verifyUserToken(std::size_t uuid, const std::string& tokens);

private:
  /*redis*/
  const std::string redis_server_login = "redis_server";

  /*user token predix*/
  const std::string token_prefix = "user_token_";

  std::mutex grpc_mtx, resources_mtx;
  std::unordered_map<
      /*server name*/ std::string,
      /*server info*/ std::unique_ptr<GRPCServerConfig>>
      grpc_servers;

  std::unordered_map<
      /*server name*/ std::string,
      /*server info*/ std::unique_ptr<ResourcesServerConfig>>
      resources_servers;
};
} // namespace grpc
#endif
