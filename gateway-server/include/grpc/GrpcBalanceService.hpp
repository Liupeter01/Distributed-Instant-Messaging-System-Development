#pragma once
#ifndef GRPCBALANCESERVICE_HPP_
#define GRPCBALANCESERVICE_HPP_
#include <grpc/BalanceServicePool.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <message/message.grpc.pb.h>
#include <message/message.pb.h>
#include <network/def.hpp>
#include <service/ConnectionPool.hpp>

struct gRPCBalancerService {
  // pass user's uuid parameter to the server, and returns available server
  // address to user
  static message::GetAllocatedChattingServer
  addNewUserToServer(std::size_t uuid) {
    grpc::ClientContext context;
    message::RegisterToBalancer request;
    message::GetAllocatedChattingServer response;
    request.set_uuid(uuid);

    connection::ConnectionRAII<stubpool::BalancerServicePool,
                               message::BalancerService::Stub>
        raii;

    grpc::Status status =
        raii->get()->AddNewUserToServer(&context, request, &response);

    ///*error occured*/
    if (!status.ok()) {
      response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    }
    return response;
  }

  static message::LoginChattingResponse
  userLoginToServer(std::size_t uuid, const std::string &token) {
    grpc::ClientContext context;
    message::LoginChattingServer request;
    message::LoginChattingResponse response;
    request.set_uuid(uuid);
    request.set_token(token);

    connection::ConnectionRAII<stubpool::BalancerServicePool,
                               message::BalancerService::Stub>
        raii;

    grpc::Status status =
        raii->get()->UserLoginToServer(&context, request, &response);

    ///*error occured*/
    if (!status.ok()) {
      response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    }
    return response;
  }
};

#endif // BALANCE
