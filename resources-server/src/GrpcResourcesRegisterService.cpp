#include <grpc/GrpcResourcesRegisterService.hpp>
#include <network/def.hpp>

message::PeerResponse gRPCResourcesRegisterService::getPeerResourcesServerLists(
    const std::string &cur_name) {

  grpc::ClientContext context;
  message::PeerRequest request;
  message::PeerResponse response;

  request.set_cur_server(cur_name);

  ConnectionRAII raii;
  grpc::Status status =
      raii->get()->GetInstancePeers(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::PeerResponse
gRPCResourcesRegisterService::getPeerResourcesGrpcServerLists(
    const std::string &cur_name) {

  grpc::ClientContext context;
  message::PeerRequest request;
  message::PeerResponse response;

  request.set_cur_server(cur_name);

  ConnectionRAII raii;
  grpc::Status status = raii->get()->GetGrpcPeers(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::StatusResponse
gRPCResourcesRegisterService::registerResourcesServerInstance(
    const std::string &name, const std::string &host, const std::string &port) {

  grpc::ClientContext context;
  message::RegisterRequest request;
  message::StatusResponse response;

  message::ServerInfo info;
  info.set_name(name);
  info.set_host(host);
  info.set_port(port);

  *request.mutable_info() = info;

  ConnectionRAII raii;
  grpc::Status status =
      raii->get()->RegisterInstance(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::StatusResponse
gRPCResourcesRegisterService::registerResourcesGrpcServer(
    const std::string &name, const std::string &host, const std::string &port) {

  grpc::ClientContext context;
  message::RegisterRequest request;
  message::StatusResponse response;

  message::ServerInfo info;
  info.set_name(name);
  info.set_host(host);
  info.set_port(port);

  *request.mutable_info() = info;

  ConnectionRAII raii;
  grpc::Status status = raii->get()->RegisterGrpc(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::StatusResponse
gRPCResourcesRegisterService::resourcesServerShutdown(const std::string &name) {

  grpc::ClientContext context;
  message::ShutdownRequest request;
  message::StatusResponse response;

  request.set_cur_server(name);

  ConnectionRAII raii;

  grpc::Status status =
      raii->get()->ShutdownInstance(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::StatusResponse
gRPCResourcesRegisterService::grpcResourcesServerShutdown(
    const std::string &name) {

  grpc::ClientContext context;
  message::ShutdownRequest request;
  message::StatusResponse response;

  request.set_cur_server(name);

  ConnectionRAII raii;

  grpc::Status status = raii->get()->ShutdownGrpc(&context, request, &response);

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}
