#pragma once
#ifndef _GRPCRESOURCESSERVICE_HPP_
#define _GRPCRESOURCESSERVICE_HPP_
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <message/message.grpc.pb.h>
#include <message/message.pb.h>
#include <string_view>
#include <service/ConnectionPool.hpp>
#include <grpc/ResourcesRegisterServicePool.hpp>

struct gRPCResourcesRegisterService {

          using  ConnectionRAII = 
                    connection::ConnectionRAII<stubpool::ResourcesRegisterServicePool,
                    message::ResourcesRegisterService::Stub>;

  static message::PeerResponse
  getPeerResourcesServerLists(const std::string &cur_name);

  static message::PeerResponse
  getPeerResourcesGrpcServerLists(const std::string &cur_name);

  static message::StatusResponse
  registerResourcesServerInstance(const std::string &name,
                                  const std::string &host,
                                  const std::string &port);

  static message::StatusResponse
  registerResourcesGrpcServer(const std::string &name, const std::string &host,
                              const std::string &port);

  static message::StatusResponse
  resourcesServerShutdown(const std::string &name);

  static message::StatusResponse
  grpcResourcesServerShutdown(const std::string &name);
};

#endif
