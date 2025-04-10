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
  // pass user's uuid parameter to the server, and returns available server
  // address to user
  static message::UserRegisterResponse addNewUserToServer(std::size_t uuid) {

    grpc::ClientContext context;
    message::UserRegisterRequest request;
    message::UserRegisterResponse response;
    request.set_uuid(uuid);

    connection::ConnectionRAII<stubpool::UserServicePool,
                               message::UserService::Stub>
        raii;

    grpc::Status status =
        raii->get()->RegisterUser(&context, request, &response);

    ///*error occured*/
    if (!status.ok()) {
      response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    }
    return response;
  }

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
};

#endif // BALANCE
