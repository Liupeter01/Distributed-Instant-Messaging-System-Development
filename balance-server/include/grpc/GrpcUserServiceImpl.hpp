#pragma once
#ifndef _GRPCUSERSERVICEIMPL_HPP_
#define _GRPCUSERSERVICEIMPL_HPP_
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>

namespace grpc {
class GrpcUserServiceImpl final : public message::UserService::Service {
public:
  GrpcUserServiceImpl();
  virtual ~GrpcUserServiceImpl() = default;

public:
  // Register new user UUID and get allocated chatting server
  virtual ::grpc::Status
  RegisterUser(::grpc::ServerContext *context,
               const ::message::UserRegisterRequest *request,
               ::message::UserRegisterResponse *response);
  // User login
  virtual ::grpc::Status LoginUser(::grpc::ServerContext *context,
                                   const ::message::LoginRequest *request,
                                   ::message::LoginResponse *response);

  // User logout
  virtual ::grpc::Status LogoutUser(::grpc::ServerContext *context,
                                    const ::message::LogoutRequest *request,
                                    ::message::LogoutResponse *response);
};
} // namespace grpc

#endif // _GRPCUSERSERVICEIMPL_HPP_
