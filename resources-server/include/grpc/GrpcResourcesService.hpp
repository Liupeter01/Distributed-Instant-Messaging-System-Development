#pragma once
#ifndef _GRPCRESOURCESSERVICE_HPP_
#define _GRPCRESOURCESSERVICE_HPP_
#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <message/message.grpc.pb.h>
#include <message/message.pb.h>
#include <string_view>

struct gRPCResourcesService {
          static message::PeerResponse
                    getPeerResourcesServerLists(const std::string& cur_name);

          static message::PeerResponse
                    getPeerResourcesGrpcServerLists(const std::string& cur_name);

          static message::GrpcStatusResponse
                    registerResourcesServerInstance(const std::string& name,
                              const std::string& host,
                              const std::string& port);

          static message::GrpcStatusResponse
                    registerResourcesGrpcServer(const std::string& name, const std::string& host,
                              const std::string& port);

          static message::GrpcStatusResponse
                    resourcesServerShutdown(const std::string& name);

          static message::GrpcStatusResponse
                    grpcResourcesServerShutdown(const std::string& name);
};

#endif 