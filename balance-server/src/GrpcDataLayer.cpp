#include <config/ServerConfig.hpp>
#include <grpc/GrpcDataLayer.hpp>
#include <redis/RedisManager.hpp>

grpc::details::ServerInstanceConf::ServerInstanceConf(const std::string &host,
                                                      const std::string &port,
                                                      const std::string &name)
    : _connections(0), _host(host), _port(port), _name(name) {}

grpc::details::GRPCServerConf::GRPCServerConf(const std::string &host,
                                              const std::string &port,
                                              const std::string &name)
    : _host(host), _port(port), _name(name) {}

std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
grpc::details::GrpcDataLayer::serverInstanceLoadBalancer(SERVER_TYPE type) {
  if (type == SERVER_TYPE::CHATTING_SERVER_INSTANCE) {
    return chattingInstanceLoadBalancer();
  } else if (type == SERVER_TYPE::RESOURCES_SERVER_INSTANCE) {
    return resourcesInstanceLoadBalancer();
  }
  return std::nullopt;
}

bool grpc::details::GrpcDataLayer::removeItemFromServer(
    const std::string &server_name, SERVER_TYPE type) {
  if (type == SERVER_TYPE::CHATTING_SERVER_INSTANCE) {
    return removeItemFromConcurrentHashMap<InstancesMappingType>(
        m_chattingServerInstances, server_name);
  } else if (type == SERVER_TYPE::RESOURCES_SERVER_INSTANCE) {
    return removeItemFromConcurrentHashMap<InstancesMappingType>(
        m_resourcesServerInstances, server_name);
  } else if (type == SERVER_TYPE::CHATTING_GRPC_SERVER) {
    return removeItemFromConcurrentHashMap<GrpcMappingType>(
        m_chattingGRPCServer, server_name);
  } else if (type == SERVER_TYPE::RESOURCES_GRPC_SERVER) {
    return removeItemFromConcurrentHashMap<GrpcMappingType>(
        m_resourcesGRPCServer, server_name);
  }
  return false;
}

bool grpc::details::GrpcDataLayer::findItemFromServer(
    const std::string &server_name, SERVER_TYPE type) {
  if (type == SERVER_TYPE::CHATTING_SERVER_INSTANCE) {
    return findItemFromServer<InstancesMappingType>(m_chattingServerInstances,
                                                    server_name);
  } else if (type == SERVER_TYPE::RESOURCES_SERVER_INSTANCE) {
    return findItemFromServer<InstancesMappingType>(m_resourcesServerInstances,
                                                    server_name);
  } else if (type == SERVER_TYPE::CHATTING_GRPC_SERVER) {
    return findItemFromServer<GrpcMappingType>(m_chattingGRPCServer,
                                               server_name);
  } else if (type == SERVER_TYPE::RESOURCES_GRPC_SERVER) {
    return findItemFromServer<GrpcMappingType>(m_resourcesGRPCServer,
                                               server_name);
  }
  return false;
}

std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
grpc::details::GrpcDataLayer::chattingInstanceLoadBalancer() {
  /*Currently, No chatting server connected!*/
  if (!m_chattingServerInstances.size()) {
    return std::nullopt;
  }

  /*remember the lowest load server in iterator*/
  decltype(m_chattingServerInstances)::iterator min_server =
      m_chattingServerInstances.begin();

  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*find key = login and field = server_name in redis, HGET*/
  std::optional<std::string> counter =
      raii->get()->getValueFromHash(redis_server_login, min_server->first);

  /*
   * if redis doesn't have this key&field in DB, then set the max value
   * or retrieve the counter number from Mem DB
   */
  min_server->second->_connections =
      !counter.has_value() ? INT_MAX : std::stoi(counter.value());

  /*for loop all the servers(including peer server)*/
  for (auto server = m_chattingServerInstances.begin();
       server != m_chattingServerInstances.end(); ++server) {

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
  return std::make_shared<grpc::details::ServerInstanceConf>(
      min_server->second->_host, min_server->second->_port,
      min_server->second->_name);
}

std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
grpc::details::GrpcDataLayer::resourcesInstanceLoadBalancer() {
  return std::nullopt;
}

void grpc::details::GrpcDataLayer::registerUserInfo(const std::size_t uuid,
                                                    const std::string &tokens) {

  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*find key = token_predix + uuid in redis, GET*/
  if (!raii->get()->setValue(token_prefix + std::to_string(uuid), tokens)) {
  }
}

/*get user token from Redis*/
std::optional<std::string>
grpc::details::GrpcDataLayer::getUserToken(const std::size_t uuid) {

  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*find key = token_predix + uuid in redis, GET*/
  std::optional<std::string> counter =
      raii->get()->checkValue(token_prefix + std::to_string(uuid));

  return counter;
}

ServiceStatus
grpc::details::GrpcDataLayer::verifyUserToken(const std::size_t uuid,
                                              const std::string &tokens) {

  auto target = getUserToken(uuid);
  if (!target.has_value()) {
    return ServiceStatus::LOGIN_UNSUCCESSFUL;
  }

  return (target.value() == tokens ? ServiceStatus::SERVICE_SUCCESS
                                   : ServiceStatus::LOGIN_INFO_ERROR);
}
