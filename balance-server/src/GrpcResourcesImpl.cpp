#include "spdlog/spdlog.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <config/ServerConfig.hpp>
#include <grpc/GrpcResourcesImpl.hpp>
#include <mutex>
#include <redis/RedisManager.hpp>

grpc::GrpcResourcesImpl::GrpcResourcesImpl() {}

grpc::GrpcResourcesImpl::~GrpcResourcesImpl() {}

std::optional<std::shared_ptr<grpc::GrpcResourcesImpl::ResourcesServerConfig>>
grpc::GrpcResourcesImpl::serverLoadBalancer() {

  /*Currently, No resources server connected!*/
  if (!resources_servers.size()) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> _lckg(resources_mtx);

  /*remember the lowest load server in iterator*/
  decltype(resources_servers)::iterator min_server = resources_servers.begin();

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
  for (auto server = resources_servers.begin();
       server != resources_servers.end(); ++server) {

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
  return std::make_shared<grpc::GrpcResourcesImpl::ResourcesServerConfig>(
      min_server->second->_host, min_server->second->_port,
      min_server->second->_name);
}

// std::optional<std::string>
// grpc::GrpcResourcesImpl::getUserToken(std::size_t uuid) {
//
//   connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
//       raii;
//
//   /*find key = token_predix + uuid in redis, GET*/
//   std::optional<std::string> counter =
//       raii->get()->checkValue(token_prefix + std::to_string(uuid));
//
//   return counter;
// }
//
// ServiceStatus
// grpc::GrpcResourcesImpl::verifyUserToken(std::size_t uuid,
//                                         const std::string &tokens) {
//   auto target = getUserToken(uuid);
//   if (!target.has_value()) {
//     return ServiceStatus::LOGIN_UNSUCCESSFUL;
//   }
//
//   return (target.value() == tokens ? ServiceStatus::SERVICE_SUCCESS
//                                    : ServiceStatus::LOGIN_INFO_ERROR);
// }
//
// void grpc::GrpcBalancerImpl::registerUserInfo(std::size_t uuid,
//                                               std::string &&tokens) {
//   connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
//       raii;
//
//   /*find key = token_predix + uuid in redis, GET*/
//   if (!raii->get()->setValue(token_prefix + std::to_string(uuid), tokens)) {
//   }
// }

::grpc::Status grpc::GrpcResourcesImpl::GetPeerResourceServerInfo(
    ::grpc::ServerContext *context, const ::message::PeerListsRequest *request,
    ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote Resources Server {0} Request To "
               "GetPeerResourcesServerInfo",
               request->cur_server());

  {
    std::lock_guard<std::mutex> _lckg(this->resources_mtx);
    auto target = this->resources_servers.find(request->cur_server());

    /*we found it in the structure*/
    if (target != this->resources_servers.end()) {
      std::for_each(
          resources_servers.begin(), resources_servers.end(),
          [&request, &response](decltype(*resources_servers.begin()) &peer) {
            if (peer.first != request->cur_server()) {
              auto new_peer = response->add_lists();
              new_peer->set_name(peer.second->_name);
              new_peer->set_host(peer.second->_host);
              new_peer->set_port(peer.second->_port);
            }
          });
    }
    spdlog::info("[Balance Server]: Remote Resources Server {0} Request To "
                 "GetPeerResourcesServerInfo Successful!",
                 request->cur_server());

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
    return grpc::Status::OK;
  }

  /*we didn't find cur_server in unordered_map*/
  spdlog::warn(
      "[Balance Server]: Remote Resources Server {0} Request To "
      "Retrieve Data From GetPeerResourcesServerInfo Failed! Because of {1}",
      request->cur_server(), "No Current Resources Server Found!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::CHATTING_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::GetPeerResourceGrpcServerInfo(
    ::grpc::ServerContext *context, const ::message::PeerListsRequest *request,
    ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote Resources Server {0} Request To "
               "GetPeerResourceGrpcServerInfo",
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
    spdlog::info("[Balance Server]: Remote Resources Server {0} Request To "
                 "GetPeerResourceGrpcServerInfo Successful!",
                 request->cur_server());

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
    return grpc::Status::OK;
  }

  /*we didn't find cur_server in unordered_map*/
  spdlog::warn(
      "[Balance Server]: Remote Resources Server {0} Request To "
      "Retrieve Data From GetPeerResourceGrpcServerInfo Failed! Because of {1}",
      request->cur_server(), "No Current GRPCServer Found!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::RegisterResourceServerInstance(
    ::grpc::ServerContext *context,
    const ::message::GrpcRegisterRequest *request,
    ::message::GrpcStatusResponse *response) {

  spdlog::info("[Balance Server]: Remote Resources Server {0} Request To "
               "Register ResourcesServerInstance.",
               request->info().name());

  {
    std::lock_guard<std::mutex> _lckg(this->resources_mtx);
    auto target = this->resources_servers.find(request->info().name());
    if (target == this->resources_servers.end()) {
      auto chatting_peer =
          std::make_unique<grpc::GrpcResourcesImpl::ResourcesServerConfig>(
              request->info().host(), request->info().port(),
              request->info().name());

      this->resources_servers.insert(
          std::pair<
              std::string,
              std::unique_ptr<grpc::GrpcResourcesImpl::ResourcesServerConfig>>(
              request->info().name(), std::move(chatting_peer)));

      spdlog::info(
          "[Balance Server]: Remote Resources Server {0} Register Host "
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
      "[Balance Server]: Remote Resources Server {0} Register Instance "
      "To ResourcesServerInstance Failed"
      "Because of {1}",
      request->info().host(), " Resources Server Already Exists!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::CHATTING_SERVER_ALREADY_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::RegisterResourceGrpcServer(
    ::grpc::ServerContext *context,
    const ::message::GrpcRegisterRequest *request,
    ::message::GrpcStatusResponse *response) {

  spdlog::info("[Balance Server]: Remote Resources GRPC Server {0} Request To "
               "Register ResourcesGrpcInstance.",
               request->info().name());

  {
    std::lock_guard<std::mutex> _lckg(this->grpc_mtx);
    auto target = this->grpc_servers.find(request->info().name());

    /*we didn't find this grpc server's name in balancer-server so its alright*/
    if (target == this->grpc_servers.end()) {
      auto grpc_peer =
          std::make_unique<grpc::GrpcResourcesImpl::GRPCServerConfig>(
              request->info().host(), request->info().port(),
              request->info().name());

      this->grpc_servers.insert(
          std::pair<std::string, std::unique_ptr<GRPCServerConfig>>(
              request->info().name(), std::move(grpc_peer)));

      spdlog::info(
          "[Balance Server]: Remote Resources GRPC Server {0} Register Host "
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
      "[Balance Server]: Remote Resources GRPC Server {0} Register Instance "
      "To ResourcesGRPCServerInstance Failed"
      "Because of {1}",
      request->info().host(), " Resources Server Already Exists!");

  response->set_error(
      static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_ALREADY_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::ResourceServerShutDown(
    ::grpc::ServerContext *context,
    const ::message::GrpcShutdownRequest *request,
    ::message::GrpcStatusResponse *response) {

  /*is both operation success or not*/
  bool status = true;

  {
    std::lock_guard<std::mutex> _lckg(this->resources_mtx);
    auto target = this->resources_servers.find(request->cur_server());
    if (target == this->resources_servers.end()) {
      status = false;
      spdlog::warn(
          "[Balance Server]: Resource GRPC Peer Server {} Remove Failed",
          request->cur_server());
    } else {
      this->resources_servers.erase(target);
    }
  }

  spdlog::info("[Balance Server]: Remote Resources Server {} Removing "
               "ResourcesServer Instance Successful",
               request->cur_server());

  response->set_error(static_cast<std::size_t>(
      status ? ServiceStatus::SERVICE_SUCCESS
             : ServiceStatus::CHATTING_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::ResourceGrpcServerShutDown(
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
      spdlog::warn(
          "[Balance Server]: Resource GRPC Peer Server {} Remove Failed",
          request->cur_server());
    } else {
      this->grpc_servers.erase(target);
    }
  }

  spdlog::info("[Balance Server]: Remote Resource Grpc Server {} "
               "Instance Successful",
               request->cur_server());

  response->set_error(
      static_cast<std::size_t>(status ? ServiceStatus::SERVICE_SUCCESS
                                      : ServiceStatus::GRPC_SERVER_NOT_EXISTS));
  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::RegisterInstance(::grpc::ServerContext* context,
          const ::message::RegisterRequest* request,
          ::message::StatusResponse* response) {

}

::grpc::Status grpc::GrpcResourcesImpl::RegisterGrpc(::grpc::ServerContext* context,
          const ::message::RegisterRequest* request,
          ::message::StatusResponse* response) {
}

::grpc::Status grpc::GrpcResourcesImpl::ShutdownInstance(::grpc::ServerContext* context,
          const ::message::ShutdownRequest* request,
          ::message::StatusResponse* response) {

}

::grpc::Status grpc::GrpcResourcesImpl::ShutdownGrpc(::grpc::ServerContext* context,
          const ::message::ShutdownRequest* request,
          ::message::StatusResponse* response) {

}

::grpc::Status grpc::GrpcResourcesImpl::GetInstancePeers(::grpc::ServerContext* context,
          const ::message::PeerRequest* request,
          ::message::PeerResponse* response) {

}

::grpc::Status grpc::GrpcResourcesImpl::GetGrpcPeers(::grpc::ServerContext* context,
          const ::message::PeerRequest* request,
          ::message::PeerResponse* response) {

}