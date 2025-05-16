#include <config/ServerConfig.hpp>
#include <grpc/GrpcDataLayer.hpp>
#include <grpc/GrpcResourcesImpl.hpp>
#include <redis/RedisManager.hpp>
#include <spdlog/spdlog.h>

grpc::GrpcResourcesImpl::GrpcResourcesImpl() {
  spdlog::info(
      "[Balance Server]: Start To Receive Resources Servers' Configrations");
}

::grpc::Status grpc::GrpcResourcesImpl::RegisterInstance(
    ::grpc::ServerContext *context, const ::message::RegisterRequest *request,
    ::message::StatusResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To Register Resources "
               "Instance Server",
               request->info().name());

  // Already Exist
  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->info().name(),
          details::SERVER_TYPE::RESOURCES_SERVER_INSTANCE)) {

    /*server has already exists*/
    spdlog::warn(
        "[Balance Server]: Remote {0} Try To Register Resources Instance Failed"
        " Because of Resources Instance Server Has Already Exists!",
        request->info().host());

    response->set_error(static_cast<std::size_t>(
        ServiceStatus::RESOURCES_SERVER_ALREADY_EXISTS));

    return grpc::Status::OK;
  }

  /*Because We have already know it is exist or not!*/
  auto &lists =
      details::GrpcDataLayer::get_instance()->getResourcesServerInstances();

  auto resources_peer = std::make_unique<grpc::details::ServerInstanceConf>(
      request->info().host(), request->info().port(), request->info().name());

  lists.insert(
      std::make_pair(request->info().name(), std::move(resources_peer)));

  spdlog::info("[Balance Server]: Remote  [{0}-{1}:{2}] Registered To "
               "Resources Instances Server Successful!",
               request->info().name(), request->info().host(),
               request->info().port());

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcResourcesImpl::RegisterGrpc(::grpc::ServerContext *context,
                                      const ::message::RegisterRequest *request,
                                      ::message::StatusResponse *response) {

  spdlog::info(
      "[Balance Server]: Remote {0} Request To Register Resources GRPC Server",
      request->info().name());

  // Already Exist
  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->info().name(),
          details::SERVER_TYPE::RESOURCES_GRPC_SERVER)) {

    /*server has already exists*/
    spdlog::warn("[Balance Server]: Remote {0} Try To Register Resources GRPC "
                 "Server Failed"
                 " Because of Resources GRPC Server Has Already Exists!",
                 request->info().host());

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_ALREADY_EXISTS));

    return grpc::Status::OK;
  }

  /*Because We have already know it is exist or not!*/
  auto &lists =
      details::GrpcDataLayer::get_instance()->getResourcesGRPCServer();

  auto grpc_peer = std::make_unique<grpc::details::GRPCServerConf>(
      request->info().host(), request->info().port(), request->info().name());

  lists.insert(std::make_pair(request->info().name(), std::move(grpc_peer)));

  spdlog::info("[Balance Server]: Remote  [{0}-{1}:{2}] Registered To "
               "Resources GRPC Server Successful!",
               request->info().name(), request->info().host(),
               request->info().port());

  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcResourcesImpl::ShutdownInstance(
    ::grpc::ServerContext *context, const ::message::ShutdownRequest *request,
    ::message::StatusResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To Remove Resources "
               "Instance Server",
               request->cur_server());

  bool status = details::GrpcDataLayer::get_instance()->removeItemFromServer(
      request->cur_server(), details::SERVER_TYPE::RESOURCES_SERVER_INSTANCE);

  if (status) {
    spdlog::info("[Balance Server]: Remote [{0}] Removed From Resources "
                 "Instance Server Successful!",
                 request->cur_server());
  } else {
    spdlog::warn("[Balance Server]: Remote [{0}] Removed From Resources "
                 "Instance Server Failed!",
                 request->cur_server());
  }

  response->set_error(static_cast<std::size_t>(
      status ? ServiceStatus::SERVICE_SUCCESS
             : ServiceStatus::RESOURCES_SERVER_NOT_EXISTS));

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcResourcesImpl::ShutdownGrpc(::grpc::ServerContext *context,
                                      const ::message::ShutdownRequest *request,
                                      ::message::StatusResponse *response) {

  spdlog::info(
      "[Balance Server]: Remote {0} Request To Remove Resources GRPC Server",
      request->cur_server());

  bool status = details::GrpcDataLayer::get_instance()->removeItemFromServer(
      request->cur_server(), details::SERVER_TYPE::RESOURCES_GRPC_SERVER);

  if (status) {
    spdlog::info("[Balance Server]: Remote [{0}] Removed From Resources GRPC "
                 "Server Successful!",
                 request->cur_server());
  } else {
    spdlog::warn("[Balance Server]: Remote [{0}] Removed From Resources GRPC "
                 "Server Failed!",
                 request->cur_server());
  }

  response->set_error(
      static_cast<std::size_t>(status ? ServiceStatus::SERVICE_SUCCESS
                                      : ServiceStatus::GRPC_SERVER_NOT_EXISTS));

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcResourcesImpl::GetInstancePeers(::grpc::ServerContext *context,
                                          const ::message::PeerRequest *request,
                                          ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To "
               "Get Other Resources Servers' Address Info",
               request->cur_server());

  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->cur_server(),
          details::SERVER_TYPE::RESOURCES_SERVER_INSTANCE)) {

    auto &lists =
        details::GrpcDataLayer::get_instance()->getResourcesServerInstances();

    for (auto it = lists.cbegin(); it != lists.cend(); ++it) {

      if (it->first == request->cur_server()) {
        continue;
      }

      auto *new_peer = response->add_lists();
      new_peer->set_name(it->second->_name);
      new_peer->set_host(it->second->_host);
      new_peer->set_port(it->second->_port);
    }

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));

    spdlog::info("[Balance Server]: Remote {0} Request To "
                 "Get Other Resources Servers' Address Info Successful!",
                 request->cur_server());
  } else {
    response->set_error(
        static_cast<std::size_t>(ServiceStatus::RESOURCES_SERVER_NOT_EXISTS));

    spdlog::warn("[Balance Server]: Remote {0} Request To "
                 "Retrieve Data From Resources Server Instance Failed! Because "
                 "of Nothing Found!",
                 request->cur_server());
  }

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcResourcesImpl::GetGrpcPeers(::grpc::ServerContext *context,
                                      const ::message::PeerRequest *request,
                                      ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To "
               "Get Other Resources GRPC Servers' Address Info",
               request->cur_server());

  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->cur_server(), details::SERVER_TYPE::RESOURCES_GRPC_SERVER)) {

    auto &lists =
        details::GrpcDataLayer::get_instance()->getResourcesGRPCServer();

    for (auto it = lists.cbegin(); it != lists.cend(); ++it) {

      if (it->first == request->cur_server()) {
        continue;
      }

      auto *new_peer = response->add_lists();
      new_peer->set_name(it->second->_name);
      new_peer->set_host(it->second->_host);
      new_peer->set_port(it->second->_port);
    }

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));

    spdlog::info("[Balance Server]: Remote {0} Request To "
                 "Get Other Resources GRPC Servers' Address Info Successful!",
                 request->cur_server());
  } else {
    response->set_error(
        static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_NOT_EXISTS));

    spdlog::warn(
        "[Balance Server]: Remote {0} Request To "
        "Retrieve Data From Resources GRPC Failed! Because of Nothing Found!",
        request->cur_server());
  }

  return grpc::Status::OK;
}
