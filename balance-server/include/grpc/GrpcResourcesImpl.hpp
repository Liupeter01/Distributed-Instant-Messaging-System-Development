#pragma once
#ifndef _GRPCRESOURCESIMPL_HPP_
#define _GRPCRESOURCESIMPL_HPP_
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>

namespace grpc {

class GrpcResourcesImpl final
    : public message::ResourcesRegisterService::Service {

public:
  GrpcResourcesImpl();
  virtual ~GrpcResourcesImpl() = default;

public:
  virtual ::grpc::Status
  RegisterInstance(::grpc::ServerContext *context,
                   const ::message::RegisterRequest *request,
                   ::message::StatusResponse *response);
  virtual ::grpc::Status RegisterGrpc(::grpc::ServerContext *context,
                                      const ::message::RegisterRequest *request,
                                      ::message::StatusResponse *response);
  virtual ::grpc::Status
  ShutdownInstance(::grpc::ServerContext *context,
                   const ::message::ShutdownRequest *request,
                   ::message::StatusResponse *response);
  virtual ::grpc::Status ShutdownGrpc(::grpc::ServerContext *context,
                                      const ::message::ShutdownRequest *request,
                                      ::message::StatusResponse *response);
  virtual ::grpc::Status GetInstancePeers(::grpc::ServerContext *context,
                                          const ::message::PeerRequest *request,
                                          ::message::PeerResponse *response);
  virtual ::grpc::Status GetGrpcPeers(::grpc::ServerContext *context,
                                      const ::message::PeerRequest *request,
                                      ::message::PeerResponse *response);
};
} // namespace grpc
#endif
