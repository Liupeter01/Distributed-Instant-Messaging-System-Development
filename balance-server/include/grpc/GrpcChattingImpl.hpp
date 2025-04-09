#pragma once
#ifndef _GRPCCHATTINGIMPL_HPP_
#define _GRPCCHATTINGIMPL_HPP_
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>
#include <grpc/GrpcDataLayer.hpp>

namespace grpc {
          class GrpcChattingImpl final : public message::ChattingRegisterService::Service {
          public:
                   ::grpc::Status RegisterInstance(::grpc::ServerContext* context, const ::message::RegisterRequest* request, ::message::StatusResponse* response)override;
                   ::grpc::Status RegisterGrpc(::grpc::ServerContext* context, const ::message::RegisterRequest* request, ::message::StatusResponse* response)override;
                    ::grpc::Status ShutdownInstance(::grpc::ServerContext* context, const ::message::ShutdownRequest* request, ::message::StatusResponse* response)override;
                  ::grpc::Status ShutdownGrpc(::grpc::ServerContext* context, const ::message::ShutdownRequest* request, ::message::StatusResponse* response)override;
                   ::grpc::Status GetInstancePeers(::grpc::ServerContext* context, const ::message::PeerRequest* request, ::message::PeerResponse* response)override;
                    ::grpc::Status GetGrpcPeers(::grpc::ServerContext* context, const ::message::PeerRequest* request, ::message::PeerResponse* response) override;
          };
}


#endif //_GRPCCHATTINGIMPL_HPP_