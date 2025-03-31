#include "spdlog/spdlog.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <config/ServerConfig.hpp>
#include <grpc/GrpcBalancerImpl.hpp>
#include <mutex>
#include <redis/RedisManager.hpp>

grpc::GrpcBalancerImpl::UserInfo::UserInfo(
    std::string &&tokens,
    const grpc::GrpcBalancerImpl::ChattingServerConfig &config)
    : m_tokens(std::move(tokens)), m_host(config._host), m_port(config._port) {}

grpc::GrpcBalancerImpl::GrpcBalancerImpl() {
  spdlog::info(
      "[Balance Server]: Start to receive Chatting Grpc Server Configrations");
}

grpc::GrpcBalancerImpl::~GrpcBalancerImpl() {}

std::optional<std::shared_ptr<grpc::GrpcBalancerImpl::ChattingServerConfig>>
grpc::GrpcBalancerImpl::serverLoadBalancer() {

  /*Currently, No chatting server connected!*/
  if (!chatting_servers.size()) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> _lckg(chatting_mtx);

  /*remember the lowest load server in iterator*/
  decltype(chatting_servers)::iterator min_server = chatting_servers.begin();

  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*find key = login and field = server_name in redis, HGET*/
  std::optional<std::string> counter =
      raii->get()->getValueFromHash(redis_server_login, min_server->first);

  /*
   * if redis doesn't have this key&field in DB, then set the max value

   * * or retrieve the counter number from Mem DB
   */
  min_server->second->_connections =
      !counter.has_value() ? INT_MAX : std::stoi(counter.value());

  /*for loop all the servers(including peer server)*/
  for (auto server = chatting_servers.begin(); server != chatting_servers.end();
       ++server) {

    /*ignore current */
    if (server->first != min_server->first) {
      std::optional<std::string> counter =
          raii->get()->getValueFromHash(redis_server_login, server->first);

      /*
       * if redis doesn't have this key&field in DB, then set the max
       * value
       * or retrieve the counter number from Mem DB
       */
      server->second->_connections =
          !counter.has_value() ? INT_MAX : std::stoi(counter.value());

      if (server->second->_connections < min_server->second->_connections) {
        min_server = server;
      }
    }
  }
  return std::make_shared<grpc::GrpcBalancerImpl::ChattingServerConfig>(
      min_server->second->_host, min_server->second->_port,
      min_server->second->_name);
}

std::optional<std::string>
grpc::GrpcBalancerImpl::getUserToken(std::size_t uuid) {

  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*find key = token_predix + uuid in redis, GET*/
  std::optional<std::string> counter =
      raii->get()->checkValue(token_prefix + std::to_string(uuid));

  return counter;
}

ServiceStatus
grpc::GrpcBalancerImpl::verifyUserToken(std::size_t uuid,
                                        const std::string &tokens) {
  auto target = getUserToken(uuid);
  if (!target.has_value()) {
    return ServiceStatus::LOGIN_UNSUCCESSFUL;
  }

  return (target.value() == tokens ? ServiceStatus::SERVICE_SUCCESS
                                   : ServiceStatus::LOGIN_INFO_ERROR);
}

void grpc::GrpcBalancerImpl::registerUserInfo(std::size_t uuid,
                                              std::string &&tokens) {
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*find key = token_predix + uuid in redis, GET*/
  if (!raii->get()->setValue(token_prefix + std::to_string(uuid), tokens)) {
  }
}

::grpc::Status grpc::GrpcBalancerImpl::AddNewUserToServer(
    ::grpc::ServerContext *context,
    const ::message::RegisterToBalancer *request,
    ::message::GetAllocatedChattingServer *response) {

  /*get the lowest load server*/
  auto target_op = serverLoadBalancer();

  /*if serverloadbalancer returns a nullopt, then it means that there is no
   * avaibale chatting-server right now!*/
  if (!target_op.has_value()) {
    response->set_error(
        static_cast<std::size_t>(ServiceStatus::NO_AVAILABLE_CHATTING_SERVER));
    return grpc::Status::OK;
  }

  auto target = target_op.value();
  response->set_error(static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
  response->set_host(target->_host);
  response->set_port(target->_port);

  auto uuid = request->uuid();
  std::optional<std::string> exists = getUserToken(uuid);

  /*check if it is registered?*/
  if (!exists.has_value()) {

    /*generate a user token and store it into the structure first*/
    std::string token = userTokenGenerator();
    response->set_token(token);
    registerUserInfo(uuid, std::move(token));
  } else {
    response->set_token(exists.value());
  }
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::UserLoginToServer(
    ::grpc::ServerContext *context,
    const ::message::LoginChattingServer *request,
    ::message::LoginChattingResponse *response) {
  /*verify user token*/
  response->set_error(static_cast<std::size_t>(
      verifyUserToken(request->uuid(), request->token())));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::GetPeerChattingServerInfo(
    ::grpc::ServerContext *context, const ::message::PeerListsRequest *request,
    ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote Chatting Server {0} Request To "
               "GetPeerChattingServerInfo",
               request->cur_server());

  {
    std::lock_guard<std::mutex> _lckg(this->chatting_mtx);
    auto target = this->chatting_servers.find(request->cur_server());

    /*we found it in the structure*/
    if (target != this->chatting_servers.end()) {
      std::for_each(
          chatting_servers.begin(), chatting_servers.end(),
          [&request, &response](decltype(*chatting_servers.begin()) &peer) {
            if (peer.first != request->cur_server()) {
              auto new_peer = response->add_lists();
              new_peer->set_name(peer.second->_name);
              new_peer->set_host(peer.second->_host);
              new_peer->set_port(peer.second->_port);
            }
          });
    }
    spdlog::info("[Balance Server]: Remote Chatting Server {0} Request To "
                 "GetPeerChattingServerInfo Successful!",
                 request->cur_server());

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
    return grpc::Status::OK;
  }

  /*we didn't find cur_server in unordered_map*/
  spdlog::warn(
      "[Balance Server]: Remote Chatting Server {0} Request To "
      "Retrieve Data From GetPeerChattingServerInfo Failed! Because of {1}",
      request->cur_server(), "No Current ChattingServer Found!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::CHATTING_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::GetPeerGrpcServerInfo(
    ::grpc::ServerContext *context, const ::message::PeerListsRequest *request,
    ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote Chatting Server {0} Request To "
               "GetPeerGrpcServerInfo",
               request->cur_server());

  {
    std::lock_guard<std::mutex> _lckg(this->grpc_mtx);
    auto target = this->grpc_servers.find(request->cur_server());

    /*we found it it mapping structure*/
    if (target != this->grpc_servers.end()) {
      std::for_each(
          grpc_servers.begin(), grpc_servers.end(),
          [&request, &response](decltype(*grpc_servers.begin()) &peer) {
            if (peer.first != request->cur_server()) {
              auto new_peer = response->add_lists();
              new_peer->set_name(peer.second->_name);
              new_peer->set_host(peer.second->_host);
              new_peer->set_port(peer.second->_port);
            }
          });
    }
    spdlog::info("[Balance Server]: Remote Chatting Server {0} Request To "
                 "GetPeerGrpcServerInfo Successful!",
                 request->cur_server());

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
    return grpc::Status::OK;
  }

  /*we didn't find cur_server in unordered_map*/
  spdlog::warn(
      "[Balance Server]: Remote Chatting Server {0} Request To "
      "Retrieve Data From GetPeerGrpcServerInfo Failed! Because of {1}",
      request->cur_server(), "No Current GRPCServer Found!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::RegisterChattingServerInstance(
    ::grpc::ServerContext *context,
    const ::message::GrpcRegisterRequest *request,
    ::message::GrpcStatusResponse *response) {

  spdlog::info("[Balance Server]: Remote Chatting Server {0} Request To "
               "Register ChattingServerInstance.",
               request->info().name());

  {
    std::lock_guard<std::mutex> _lckg(this->chatting_mtx);
    auto target = this->chatting_servers.find(request->info().name());
    if (target == this->chatting_servers.end()) {
      auto chatting_peer =
          std::make_unique<grpc::GrpcBalancerImpl::ChattingServerConfig>(
              request->info().host(), request->info().port(),
              request->info().name());

      this->chatting_servers.insert(
          std::pair<
              std::string,
              std::unique_ptr<grpc::GrpcBalancerImpl::ChattingServerConfig>>(
              request->info().name(), std::move(chatting_peer)));

      spdlog::info("[Balance Server]: Remote Chatting Server {0} Register Host "
                   "= {1}, Port = {2} Successful",
                   request->info().name(), request->info().host(),
                   request->info().port());

      response->set_error(
          static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
      return grpc::Status::OK;
    }
  }
  /*server has already exists*/
  spdlog::warn("[Balance Server]: Remote Chatting Server {0} Register Instance "
               "To ChattingServerInstance Failed"
               "Because of {1}",
               request->info().host(), "Chatting Server Already Exists!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::CHATTING_SERVER_ALREADY_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::RegisterChattingGrpcServer(
    ::grpc::ServerContext *context,
    const ::message::GrpcRegisterRequest *request,
    ::message::GrpcStatusResponse *response) {

  spdlog::info("[Balance Server]: Remote Chatting GRPC Server {0} Request To "
               "Register ChattingGrpcInstance.",
               request->info().name());

  {
    std::lock_guard<std::mutex> _lckg(this->grpc_mtx);
    auto target = this->grpc_servers.find(request->info().name());

    /*we didn't find this grpc server's name in balancer-server so its alright*/
    if (target == this->grpc_servers.end()) {
      auto grpc_peer =
          std::make_unique<grpc::GrpcBalancerImpl::GRPCServerConfig>(
              request->info().host(), request->info().port(),
              request->info().name());

      this->grpc_servers.insert(
          std::pair<std::string, std::unique_ptr<GRPCServerConfig>>(
              request->info().name(), std::move(grpc_peer)));

      spdlog::info(
          "[Balance Server]: Remote Chatting GRPC Server {0} Register Host "
          "= {1}, Port = {2} Successful",
          request->info().name(), request->info().host(),
          request->info().port());

      response->set_error(
          static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
      return grpc::Status::OK;
    }
  }

  /*server has already exists*/
  spdlog::warn(
      "[Balance Server]: Remote Chatting GRPC Server {0} Register Instance "
      "To ChattingGRPCServerInstance Failed"
      "Because of {1}",
      request->info().host(), "Chatting Server Already Exists!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_ALREADY_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::ChattingServerShutDown(
    ::grpc::ServerContext *context,
    const ::message::GrpcShutdownRequest *request,
    ::message::GrpcStatusResponse *response) {
  /*is both operation success or not*/
  bool status = true;

  {
    std::lock_guard<std::mutex> _lckg(this->chatting_mtx);
    auto target = this->chatting_servers.find(request->cur_server());
    if (target == this->chatting_servers.end()) {
      status = false;
      spdlog::warn("[Balance Server]: GRPC Peer Server {} Remove Failed",
                   request->cur_server());
    } else {
      this->chatting_servers.erase(target);
    }
  }

  spdlog::info("[Balance Server]: Remote Chatting Server {} Removing "
               "ChattingServer Instance Successful",
               request->cur_server());

  response->set_error(static_cast<std::size_t>(
      status ? ServiceStatus::SERVICE_SUCCESS
             : ServiceStatus::CHATTING_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcBalancerImpl::ChattingGrpcServerShutDown(
    ::grpc::ServerContext *context,
    const ::message::GrpcShutdownRequest *request,
    ::message::GrpcStatusResponse *response) {
  /*is both operation success or not*/
  bool status = true;

  {
    std::lock_guard<std::mutex> _lckg(this->grpc_mtx);
    auto target = this->grpc_servers.find(request->cur_server());
    if (target == this->grpc_servers.end()) {
      status = false;
      spdlog::warn("[Balance Server]: GRPC Peer Server {} Remove Failed",
                   request->cur_server());
    } else {
      this->grpc_servers.erase(target);
    }
  }

  spdlog::info("[Balance Server]: Remote Grpc Server {} Removing "
               "Grpc Server Instance Successful",
               request->cur_server());

  response->set_error(
      static_cast<std::size_t>(status ? ServiceStatus::SERVICE_SUCCESS
                                      : ServiceStatus::GRPC_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

std::string grpc::GrpcBalancerImpl::userTokenGenerator() {
  boost::uuids::uuid uuid_gen = boost::uuids::random_generator()();
  return boost::uuids::to_string(uuid_gen);
}
