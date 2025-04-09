#include <spdlog/spdlog.h>
#include <grpc/GrpcChattingImpl.hpp>
#include <grpc/GrpcDataLayer.hpp>

::grpc::Status grpc::GrpcChattingImpl::RegisterInstance(
    ::grpc::ServerContext *context, const ::message::RegisterRequest *request,
    ::message::StatusResponse *response) {
}

::grpc::Status
grpc::GrpcChattingImpl::RegisterGrpc(::grpc::ServerContext *context,
                                     const ::message::RegisterRequest *request,
                                     ::message::StatusResponse *response) {}

::grpc::Status grpc::GrpcChattingImpl::ShutdownInstance(
    ::grpc::ServerContext *context, const ::message::ShutdownRequest *request,
    ::message::StatusResponse *response) {}

::grpc::Status
grpc::GrpcChattingImpl::ShutdownGrpc(::grpc::ServerContext *context,
                                     const ::message::ShutdownRequest *request,
                                     ::message::StatusResponse *response) {}

::grpc::Status
grpc::GrpcChattingImpl::GetInstancePeers(::grpc::ServerContext *context,
                                         const ::message::PeerRequest *request,
                                         ::message::PeerResponse *response) {}

::grpc::Status
grpc::GrpcChattingImpl::GetGrpcPeers(::grpc::ServerContext *context,
                                     const ::message::PeerRequest *request,
                                     ::message::PeerResponse *response) {}
