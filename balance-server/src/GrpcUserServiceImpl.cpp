#include <grpc/GrpcDataLayer.hpp>
#include <grpc/GrpcUserServiceImpl.hpp>
#include <tools/tools.hpp>

grpc::GrpcUserServiceImpl::GrpcUserServiceImpl() {
  spdlog::info("[Balance Server]: Start To Receive User Requests");
}

// Register new user UUID and get allocated chatting server
::grpc::Status grpc::GrpcUserServiceImpl::RegisterUser(
    ::grpc::ServerContext *context,
    const ::message::UserRegisterRequest *request,
    ::message::UserRegisterResponse *response) {

  /*get the lowest load server*/
  auto server_opt =
      details::GrpcDataLayer::get_instance()->serverInstanceLoadBalancer(
          details::SERVER_TYPE::CHATTING_SERVER_INSTANCE);

  /*if serverloadbalancer returns a nullopt, then it means that there is no
   * avaibale chatting-server right now!*/
  if (!server_opt.has_value()) {
    response->set_error(
        static_cast<std::size_t>(ServiceStatus::NO_AVAILABLE_CHATTING_SERVER));

    return grpc::Status::OK;
  }

  response->set_error(static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS));
  response->set_host((*server_opt)->_host);
  response->set_port((*server_opt)->_port);

  // check if this user's uuid has token or not!
  std::optional<std::string> exists_opt =
      details::GrpcDataLayer::get_instance()->getUserToken(request->uuid());
  if (exists_opt.has_value()) {
    response->set_token(*exists_opt);
    return grpc::Status::OK;
  }

  /*Generate a new token, and register to data layer then return it back to
   * user!*/
  std::string token = tools::userTokenGenerator();
  details::GrpcDataLayer::get_instance()->registerUserInfo(request->uuid(),
                                                           token);

  response->set_token(token);
  return grpc::Status::OK;
}

// User login
::grpc::Status
grpc::GrpcUserServiceImpl::LoginUser(::grpc::ServerContext *context,
                                     const ::message::LoginRequest *request,
                                     message::LoginResponse *response) {

  response->set_error(static_cast<std::size_t>(
      details::GrpcDataLayer::get_instance()->verifyUserToken(
          request->uuid(), request->token())));
  return grpc::Status::OK;
}

// User logout
::grpc::Status
grpc::GrpcUserServiceImpl::LogoutUser(::grpc::ServerContext *context,
                                      const ::message::LogoutRequest *request,
                                      ::message::LogoutResponse *response) {

  response->set_error(static_cast<std::size_t>(
      details::GrpcDataLayer::get_instance()->verifyUserToken(
          request->uuid(), request->token())));
  return grpc::Status::OK;
}
