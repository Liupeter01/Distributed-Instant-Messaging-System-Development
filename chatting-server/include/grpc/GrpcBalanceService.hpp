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
#include <string_view>

struct gRPCBalancerService {

  static message::LoginChattingResponse
  userLoginToServer(std::size_t uuid, const std::string &token);

  static message::PeerResponse
  getPeerChattingServerLists(const std::string &cur_name);

  static message::PeerResponse
  getPeerGrpcServerLists(const std::string &cur_name);

  static message::GrpcStatusResponse
  registerChattingServerInstance(const std::string &name,
                                 const std::string &host,
                                 const std::string &port);

  static message::GrpcStatusResponse
  registerGrpcServer(const std::string &name, const std::string &host,
                     const std::string &port);

  static message::GrpcStatusResponse
  chattingServerShutdown(const std::string &name);

  static message::GrpcStatusResponse
  grpcServerShutdown(const std::string &name);
};

#endif // GRPCBALANCESERVICE_HPP_
