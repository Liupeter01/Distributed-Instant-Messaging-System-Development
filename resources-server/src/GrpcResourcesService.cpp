#include <grpc/GrpcResourcesService.hpp>
#include <grpc/ResourcesServicePool.hpp>
#include <network/def.hpp>
#include <service/ConnectionPool.hpp>

message::PeerResponse
gRPCResourcesService::getPeerResourcesServerLists(const std::string &cur_name) {

  grpc::ClientContext context;
  message::PeerListsRequest request;
  message::PeerResponse response;

  request.set_cur_server(cur_name);

  connection::ConnectionRAII<stubpool::ResourcesServicePool,
                             message::ResourceService::Stub>
      raii;

  grpc::Status status =
      raii->get()->GetPeerResourceServerInfo(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::PeerResponse gRPCResourcesService::getPeerResourcesGrpcServerLists(
    const std::string &cur_name) {

  grpc::ClientContext context;
  message::PeerListsRequest request;
  message::PeerResponse response;

  request.set_cur_server(cur_name);

  connection::ConnectionRAII<stubpool::ResourcesServicePool,
                             message::ResourceService::Stub>
      raii;

  grpc::Status status =
      raii->get()->GetPeerResourceGrpcServerInfo(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::GrpcStatusResponse
gRPCResourcesService::registerResourcesServerInstance(const std::string &name,
                                                      const std::string &host,
                                                      const std::string &port) {

  grpc::ClientContext context;
  message::GrpcRegisterRequest request;
  message::GrpcStatusResponse response;

  message::ServerInfo info;
  info.set_name(name);
  info.set_host(host);
  info.set_port(port);

  *request.mutable_info() = info;

  connection::ConnectionRAII<stubpool::ResourcesServicePool,
                             message::ResourceService::Stub>
      raii;

  grpc::Status status =
      raii->get()->RegisterResourceServerInstance(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::GrpcStatusResponse gRPCResourcesService::registerResourcesGrpcServer(
    const std::string &name, const std::string &host, const std::string &port) {

  grpc::ClientContext context;
  message::GrpcRegisterRequest request;
  message::GrpcStatusResponse response;

  message::ServerInfo info;
  info.set_name(name);
  info.set_host(host);
  info.set_port(port);

  *request.mutable_info() = info;

  connection::ConnectionRAII<stubpool::ResourcesServicePool,
                             message::ResourceService::Stub>
      raii;

  grpc::Status status =
      raii->get()->RegisterResourceGrpcServer(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::GrpcStatusResponse
gRPCResourcesService::resourcesServerShutdown(const std::string &name) {

  grpc::ClientContext context;
  message::GrpcShutdownRequest request;
  message::GrpcStatusResponse response;

  request.set_cur_server(name);

  connection::ConnectionRAII<stubpool::ResourcesServicePool,
                             message::ResourceService::Stub>
      raii;

  grpc::Status status =
      raii->get()->ResourceServerShutDown(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::GrpcStatusResponse
gRPCResourcesService::grpcResourcesServerShutdown(const std::string &name) {

  grpc::ClientContext context;
  message::GrpcShutdownRequest request;
  message::GrpcStatusResponse response;

  request.set_cur_server(name);

  connection::ConnectionRAII<stubpool::ResourcesServicePool,
                             message::ResourceService::Stub>
      raii;

  grpc::Status status =
      raii->get()->ResourceGrpcServerShutDown(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}
