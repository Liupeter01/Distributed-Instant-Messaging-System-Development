#include <config/ServerConfig.hpp>
#include <grpc/GrpcDistributedChattingService.hpp>
#include <grpc/GrpcRegisterChattingService.hpp>

gRPCDistributedChattingService::gRPCDistributedChattingService() {
  updateGrpcPeerLists();
}

void gRPCDistributedChattingService::updateGrpcPeerLists() {
  /*clean it first*/
  m_pools.erase(m_pools.begin(), m_pools.end());
  m_pools.clear();

  /*pass current server name as a parameter to the balance server, and returns
   * all peers*/
  auto response = gRPCGrpcRegisterChattingService::getPeerGrpcServerLists(
      ServerConfig::get_instance()->GrpcServerName);

  if (response.error() !=
      static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {

    spdlog::info(
        "[{}] Trying To Retrieve Peer Server's Info Failed, Error Code: {}",
        ServerConfig::get_instance()->GrpcServerName, response.error());

    std::abort();
  }

  /*get server lists*/
  auto &peer_servers = response.lists();

  /*traversal server lists and create multiple DistributedChattingServicePool
   * according to host and port*/
  std::for_each(
      peer_servers.begin(), peer_servers.end(),
      [this](const message::ServerInfo &server) {
        m_pools[server.name()] =
            std::make_shared<stubpool::DistributedChattingServicePool>(
                server.host(), server.port());
      });
}

std::optional<std::shared_ptr<stubpool::DistributedChattingServicePool>>
gRPCDistributedChattingService::getTargetChattingServer(
    const std::string &server_name) {
  /*user should not input current server*/
  if (ServerConfig::get_instance()->GrpcServerName == server_name) {
    return std::nullopt;
  }

  /*ask balance-server for new data*/
  updateGrpcPeerLists();

  /*remote server name not exists*/
  auto it = m_pools.find(server_name);
  if (it == m_pools.end()) {
    return std::nullopt;
  }
  return it->second;
}

message::TerminationResponse
gRPCDistributedChattingService::forceTerminateLoginedUser(
    const std::string &server_name, const message::TerminationRequest &req) {
  grpc::ClientContext context;
  message::TerminationResponse response;

  response.set_error(static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS));
  response.set_kick_uuid(req.kick_uuid());

  /*get the connection pool of this server*/
  auto server_op = getTargetChattingServer(server_name);
  // server not found
  if (!server_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: GRPC {} Not Found",
                 ServerConfig::get_instance()->GrpcServerName, server_name);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  /*get one connection stub from connection pool*/
  auto stub_op = server_op.value()->acquire_stub();

  // connection stub not found
  if (!stub_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: Connection Stub Parse Error!",
                 ServerConfig::get_instance()->GrpcServerName);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  grpc::Status status =
      (*stub_op).get()->ForceTerminateLoginedUser(&context, req, &response);

  /*return this stub back*/
  server_op.value()->release_stub(std::move(stub_op.value()));

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::FriendResponse gRPCDistributedChattingService::sendFriendRequest(
    const std::string &server_name, const message::FriendRequest &req) {
  grpc::ClientContext context;
  message::FriendResponse response;

  response.set_error(static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS));

  /*get the connection pool of this server*/
  auto server_op = getTargetChattingServer(server_name);

  // server not found
  if (!server_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: GRPC {} Not Found",
                 ServerConfig::get_instance()->GrpcServerName, server_name);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  /*get one connection stub from connection pool*/
  auto stub_op = server_op.value()->acquire_stub();

  // connection stub not found
  if (!stub_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: Connection Stub Parse Error!",
                 ServerConfig::get_instance()->GrpcServerName);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  grpc::Status status =
      stub_op.value().get()->SendFriendRequest(&context, req, &response);

  /*return this stub back*/
  server_op.value()->release_stub(std::move(stub_op.value()));

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }
  return response;
}

message::FriendResponse gRPCDistributedChattingService::confirmFriendRequest(
    const std::string &server_name, const message::FriendRequest &req) {
  grpc::ClientContext context;
  message::FriendResponse response;

  response.set_error(static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS));

  /*get the connection pool of this server*/
  auto server_op = getTargetChattingServer(server_name);

  // server not found
  if (!server_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: GRPC {} Not Found",
                 ServerConfig::get_instance()->GrpcServerName, server_name);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  /*get one connection stub from connection pool*/
  auto stub_op = server_op.value()->acquire_stub();

  // connection stub not found
  if (!stub_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: Connection Stub Parse Error!",
                 ServerConfig::get_instance()->GrpcServerName);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  grpc::Status status =
      stub_op.value().get()->ConfirmFriendRequest(&context, req, &response);

  /*return this stub back*/
  server_op.value()->release_stub(std::move(stub_op.value()));

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }

  return response;
}

message::ChattingTextMsgResponse
gRPCDistributedChattingService::sendChattingTextMsg(
    const std::string &server_name,
    const message::ChattingTextMsgRequest &req) {
  grpc::ClientContext context;
  message::ChattingTextMsgResponse response;

  response.set_error(static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS));

  /*get the connection pool of this server*/
  auto server_op = getTargetChattingServer(server_name);

  // server not found
  if (!server_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: GRPC {} Not Found",
                 ServerConfig::get_instance()->GrpcServerName, server_name);
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
    return response;
  }

  /*get one connection stub from connection pool*/
  auto stub_op = server_op.value()->acquire_stub();

  grpc::Status status =
      stub_op.value().get()->SendChattingTextMsg(&context, req, &response);

  /*return this stub back*/
  server_op.value()->release_stub(std::move(stub_op.value()));

  ///*error occured*/
  if (!status.ok()) {
    response.set_error(static_cast<int32_t>(ServiceStatus::GRPC_ERROR));
  }

  return response;
}
