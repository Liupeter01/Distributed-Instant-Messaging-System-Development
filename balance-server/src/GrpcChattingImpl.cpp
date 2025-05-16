#include <config/ServerConfig.hpp>
#include <grpc/GrpcChattingImpl.hpp>
#include <grpc/GrpcDataLayer.hpp>
#include <spdlog/spdlog.h>

grpc::GrpcChattingImpl::GrpcChattingImpl() {
  spdlog::info(
      "[Balance Server]: Start To Receive Chatting Servers' Configrations");
}

::grpc::Status grpc::GrpcChattingImpl::RegisterInstance(
    ::grpc::ServerContext *context, const ::message::RegisterRequest *request,
    ::message::StatusResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To Register Chatting "
               "Instance Server",
               request->info().name());

  // Already Exist
  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->info().name(),
          details::SERVER_TYPE::CHATTING_SERVER_INSTANCE)) {

    /*server has already exists*/
    spdlog::warn(
        "[Balance Server]: Remote {0} Try To Register Chatting Instance Failed"
        " Because of Chatting Instance Server Has Already Exists!",
        request->info().host());

    response->set_error(static_cast<std::size_t>(
        ServiceStatus::CHATTING_SERVER_ALREADY_EXISTS));

    return grpc::Status::OK;
  }

  /*Because We have already know it is exist or not!*/
  auto &lists =
      details::GrpcDataLayer::get_instance()->getChattingServerInstances();

  auto chatting_peer = std::make_unique<grpc::details::ServerInstanceConf>(
      request->info().host(), request->info().port(), request->info().name());

  lists.insert(
      std::make_pair(request->info().name(), std::move(chatting_peer)));

  spdlog::info("[Balance Server]: Remote  [{0}-{1}:{2}] Registered To Chatting "
               "Instances Server Successful!",
               request->info().name(), request->info().host(),
               request->info().port());

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcChattingImpl::RegisterGrpc(::grpc::ServerContext *context,
                                     const ::message::RegisterRequest *request,
                                     ::message::StatusResponse *response) {

  spdlog::info(
      "[Balance Server]: Remote {0} Request To Register Chatting GRPC Server",
      request->info().name());

  // Already Exist
  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->info().name(), details::SERVER_TYPE::CHATTING_GRPC_SERVER)) {

    /*server has already exists*/
    spdlog::warn("[Balance Server]: Remote {0} Try To Register Chatting GRPC "
                 "Server Failed"
                 " Because of Chatting GRPC Server Has Already Exists!",
                 request->info().host());

    response->set_error(
        static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_ALREADY_EXISTS));

    return grpc::Status::OK;
  }

  /*Because We have already know it is exist or not!*/
  auto &lists = details::GrpcDataLayer::get_instance()->getChattingGRPCServer();

  auto grpc_peer = std::make_unique<grpc::details::GRPCServerConf>(
      request->info().host(), request->info().port(), request->info().name());

  lists.insert(std::make_pair(request->info().name(), std::move(grpc_peer)));

  spdlog::info("[Balance Server]: Remote  [{0}-{1}:{2}] Registered To Chatting "
               "GRPC Server Successful!",
               request->info().name(), request->info().host(),
               request->info().port());

  return grpc::Status::OK;
}

::grpc::Status grpc::GrpcChattingImpl::ShutdownInstance(
    ::grpc::ServerContext *context, const ::message::ShutdownRequest *request,
    ::message::StatusResponse *response) {

  spdlog::info(
      "[Balance Server]: Remote {0} Request To Remove Chatting Instance Server",
      request->cur_server());

  bool status = details::GrpcDataLayer::get_instance()->removeItemFromServer(
      request->cur_server(), details::SERVER_TYPE::CHATTING_SERVER_INSTANCE);

  if (status) {
    spdlog::info("[Balance Server]: Remote [{0}] Removed From Chatting "
                 "Instance Server Successful!",
                 request->cur_server());
  } else {
    spdlog::warn("[Balance Server]: Remote [{0}] Removed From Chatting "
                 "Instance Server Failed!",
                 request->cur_server());
  }

  response->set_error(static_cast<std::size_t>(
      status ? ServiceStatus::SERVICE_SUCCESS
             : ServiceStatus::CHATTING_SERVER_NOT_EXISTS));

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcChattingImpl::ShutdownGrpc(::grpc::ServerContext *context,
                                     const ::message::ShutdownRequest *request,
                                     ::message::StatusResponse *response) {

  spdlog::info(
      "[Balance Server]: Remote {0} Request To Remove Chatting GRPC Server",
      request->cur_server());

  bool status = details::GrpcDataLayer::get_instance()->removeItemFromServer(
      request->cur_server(), details::SERVER_TYPE::CHATTING_GRPC_SERVER);

  if (status) {
    spdlog::info("[Balance Server]: Remote [{0}] Removed From Chatting GRPC "
                 "Server Successful!",
                 request->cur_server());
  } else {
    spdlog::warn("[Balance Server]: Remote [{0}] Removed From Chatting GRPC "
                 "Server Failed!",
                 request->cur_server());
  }

  response->set_error(
      static_cast<std::size_t>(status ? ServiceStatus::SERVICE_SUCCESS
                                      : ServiceStatus::GRPC_SERVER_NOT_EXISTS));

  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcChattingImpl::GetInstancePeers(::grpc::ServerContext *context,
                                         const ::message::PeerRequest *request,
                                         ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To "
               "Get Other Chatting Servers' Address Info",
               request->cur_server());

  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->cur_server(),
          details::SERVER_TYPE::CHATTING_SERVER_INSTANCE)) {

    auto &lists =
        details::GrpcDataLayer::get_instance()->getChattingServerInstances();

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
                 "Get Other Chatting Servers' Address Info Successful!",
                 request->cur_server());
  } else {
    response->set_error(
        static_cast<std::size_t>(ServiceStatus::CHATTING_SERVER_NOT_EXISTS));

    spdlog::warn("[Balance Server]: Remote {0} Request To "
                 "Retrieve Data From Chatting Server Instance Failed! Because "
                 "of Nothing Found!",
                 request->cur_server());
  }
  return grpc::Status::OK;
}

::grpc::Status
grpc::GrpcChattingImpl::GetGrpcPeers(::grpc::ServerContext *context,
                                     const ::message::PeerRequest *request,
                                     ::message::PeerResponse *response) {

  spdlog::info("[Balance Server]: Remote {0} Request To "
               "Get Other Chatting GRPC Servers' Address Info",
               request->cur_server());

  if (details::GrpcDataLayer::get_instance()->findItemFromServer(
          request->cur_server(), details::SERVER_TYPE::CHATTING_GRPC_SERVER)) {

    auto &lists =
        details::GrpcDataLayer::get_instance()->getChattingGRPCServer();

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
                 "Get Other Chatting GRPC Servers' Address Info Successful!",
                 request->cur_server());
  } else {
    response->set_error(
        static_cast<std::size_t>(ServiceStatus::GRPC_SERVER_NOT_EXISTS));

    spdlog::warn(
        "[Balance Server]: Remote {0} Request To "
        "Retrieve Data From Chatting GRPC Failed! Because of Nothing Found!",
        request->cur_server());
  }

  return grpc::Status::OK;
}
