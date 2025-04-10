#pragma once
#ifndef GRPCUSERSERVICE_HPP_
#define GRPCUSERSERVICE_HPP_
#include <grpc/UserServicePool.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <message/message.grpc.pb.h>
#include <message/message.pb.h>
#include <network/def.hpp>
#include <service/ConnectionPool.hpp>

struct gRPCGrpcUserService {

  static message::LoginResponse userLoginToServer(std::size_t uuid,
                                                  const std::string &token) {
    grpc::ClientContext context;
    message::LoginRequest request;
    message::LoginResponse response;
    request.set_uuid(uuid);
    request.set_token(token);

    connection::ConnectionRAII<stubpool::UserServicePool,
                               message::UserService::Stub>
        raii;

    grpc::Status status = raii->get()->LoginUser(&context, request, &response);

    ///*error occured*/
    if (!status.ok()) {
      response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    }
    return response;
  }

  static message::LogoutResponse
  userLogoutFromServer(const std::size_t uuid, const std::string &token) {
    grpc::ClientContext context;
    message::LogoutRequest request;
    message::LogoutResponse response;

    request.set_uuid(uuid);
    request.set_token(token);

    connection::ConnectionRAII<stubpool::UserServicePool,
                               message::UserService::Stub>
        raii;

    grpc::Status status = raii->get()->LogoutUser(&context, request, &response);

    ///*error occured*/
    if (!status.ok()) {
      response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    }
    return response;
  }
};

#endif // BALANCE
