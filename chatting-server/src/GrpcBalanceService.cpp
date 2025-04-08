#include <grpc/BalanceServicePool.hpp>
#include <grpc/GrpcBalanceService.hpp>
#include <network/def.hpp>
#include <service/ConnectionPool.hpp>

message::LoginChattingResponse
gRPCBalancerService::userLoginToServer(std::size_t uuid,
                                       const std::string &token) {
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

message::LogoutChattingResponse
gRPCBalancerService::userLogoutFromServer(std::size_t uuid, const std::string& token) {
          grpc::ClientContext context;
          message::LogoutChattingServer request;
          message::LogoutChattingResponse response;
          request.set_uuid(uuid);
          request.set_token(token);

          connection::ConnectionRAII<stubpool::BalancerServicePool,
                    message::BalancerService::Stub>
                    raii;

          grpc::Status status =
                    raii->get()->UserLogoutFromServer(&context, request, &response);

          ///*error occured*/
          if (!status.ok()) {
                    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
          }
          return response;
}

message::PeerResponse
gRPCBalancerService::getPeerChattingServerLists(const std::string &cur_name) {
  grpc::ClientContext context;
  message::PeerListsRequest request;
  message::PeerResponse response;

  request.set_cur_server(cur_name);

  connection::ConnectionRAII<stubpool::BalancerServicePool,
                             message::BalancerService::Stub>
      raii;

  grpc::Status status =
      raii->get()->GetPeerChattingServerInfo(&context, request, &response);

  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::PeerResponse
gRPCBalancerService::getPeerGrpcServerLists(const std::string &cur_name) {
  grpc::ClientContext context;
  message::PeerListsRequest request;
  message::PeerResponse response;

  request.set_cur_server(cur_name);

  connection::ConnectionRAII<stubpool::BalancerServicePool,
                             message::BalancerService::Stub>
      raii;

  grpc::Status status =
      raii->get()->GetPeerGrpcServerInfo(&context, request, &response);

  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::GrpcStatusResponse gRPCBalancerService::registerChattingServerInstance(
    const std::string &name, const std::string &host, const std::string &port) {
  grpc::ClientContext context;
  message::GrpcRegisterRequest request;
  message::GrpcStatusResponse response;

  message::ServerInfo info;
  info.set_name(name);
  info.set_host(host);
  info.set_port(port);

  *request.mutable_info() = info;

  connection::ConnectionRAII<stubpool::BalancerServicePool,
                             message::BalancerService::Stub>
      raii;

  grpc::Status status =
      raii->get()->RegisterChattingServerInstance(&context, request, &response);

  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }

  return response;
}

message::GrpcStatusResponse gRPCBalancerService::registerGrpcServer(
    const std::string &name, const std::string &host, const std::string &port) {
  grpc::ClientContext context;
  message::GrpcRegisterRequest request;
  message::GrpcStatusResponse response;

  message::ServerInfo info;
  info.set_name(name);
  info.set_host(host);
  info.set_port(port);

  *request.mutable_info() = info;

  connection::ConnectionRAII<stubpool::BalancerServicePool,
                             message::BalancerService::Stub>
      raii;

  grpc::Status status =
      raii->get()->RegisterChattingGrpcServer(&context, request, &response);

  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }

  return response;
}

message::GrpcStatusResponse
gRPCBalancerService::chattingServerShutdown(const std::string &name) {
  grpc::ClientContext context;
  message::GrpcShutdownRequest request;
  message::GrpcStatusResponse response;

  request.set_cur_server(name);

  connection::ConnectionRAII<stubpool::BalancerServicePool,
                             message::BalancerService::Stub>
      raii;

  grpc::Status status =
      raii->get()->ChattingServerShutDown(&context, request, &response);

  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::GrpcStatusResponse
gRPCBalancerService::grpcServerShutdown(const std::string &name) {
  grpc::ClientContext context;
  message::GrpcShutdownRequest request;
  message::GrpcStatusResponse response;

  request.set_cur_server(name);

  connection::ConnectionRAII<stubpool::BalancerServicePool,
                             message::BalancerService::Stub>
      raii;

  grpc::Status status =
      raii->get()->ChattingGrpcServerShutDown(&context, request, &response);

  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}
