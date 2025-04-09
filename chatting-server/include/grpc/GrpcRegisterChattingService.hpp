#pragma once
#ifndef _GRPCCHATTINGREGISTERSERVICE_HPP_
#define _GRPCCHATTINGREGISTERSERVICE_HPP_
#include <grpc/RegisterChattingServicePool.hpp>
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <message/message.grpc.pb.h>
#include <message/message.pb.h>
#include <network/def.hpp>
#include <service/ConnectionPool.hpp>

struct gRPCGrpcRegisterChattingService{

          using  ConnectionRAII = connection::ConnectionRAII<stubpool::RegisterChattingServicePool,
                    message::ChattingRegisterService::Stub>;

  static message::PeerResponse
  getPeerChattingServerLists(const std::string &cur_name);

  static message::PeerResponse
  getPeerGrpcServerLists(const std::string &cur_name);

  static message::StatusResponse
  registerChattingServerInstance(const std::string &name,
                                 const std::string &host,
                                 const std::string &port);

  static message::StatusResponse
  registerGrpcServer(const std::string &name, const std::string &host,
                     const std::string &port);

  static message::StatusResponse
  chattingServerShutdown(const std::string &name);

  static message::StatusResponse
  grpcServerShutdown(const std::string &name);
};

#endif //CHATTINGREGISTERSERVICE