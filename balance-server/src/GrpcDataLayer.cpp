#include <config/ServerConfig.hpp>
#include <grpc/GrpcDataLayer.hpp>

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
            return InstanceLoadBalancer(m_chattingServerInstances);
  } else if (type == SERVER_TYPE::RESOURCES_SERVER_INSTANCE) {
    return InstanceLoadBalancer(m_resourcesServerInstances);
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
grpc::details::GrpcDataLayer::InstanceLoadBalancer(const InstancesMappingType& map) {
          /*Currently, No server connected!*/
          if (!map.size()) {
                    return std::nullopt;
          }

          RedisRAII raii;

          /*remember the lowest load server in iterator*/
          auto min_server = map.cbegin();

          // Acquire Lock£¨we need to know how many people are in this server£©
          auto get_distributed_lock = raii->get()->acquire(
                    min_server->first, 10, 10, redis::TimeUnit::Milliseconds);

          if (!get_distributed_lock.has_value()) {
                    spdlog::error("[Balance Server] Acquire Distributed-Lock In {} Failed!",
                              min_server->first);
                    return std::nullopt;
          }

          /*find key = login and field = server_name in redis, HGET*/
          std::optional<std::string> counter =
                    raii->get()->getValueFromHash(redis_server_login, min_server->first);

          // Release Lock
          raii->get()->release(min_server->first, get_distributed_lock.value());

          /*
           * if redis doesn't have this key&field in DB, then set the max value
           * or retrieve the counter number from Mem DB
           */
          min_server->second->_connections =
                    !counter.has_value() ? std::numeric_limits<std::size_t>::max()
                    : std::stoi(counter.value());

          /*for loop all the servers(including peer server)*/
          for (auto icb = map.cbegin(); icb != map.cend(); icb++) {

                    /*ignore current */
                    if (icb->first == min_server->first) {
                              continue;
                    }

                    // Acquire Lock
                    auto get_distributed_lock = raii->get()->acquire(
                              icb->first, 10, 10, redis::TimeUnit::Milliseconds);

                    if (!get_distributed_lock.has_value()) {
                              spdlog::error("[Balance Server] Acquire Distributed-Lock In {} Failed!",
                                        icb->first);
                              return std::nullopt;
                    }

                    std::optional<std::string> counter =
                              raii->get()->getValueFromHash(redis_server_login, icb->first);

                    // Release Lock
                    raii->get()->release(icb->first, get_distributed_lock.value());

                    /*
                     * if redis doesn't have this key&field in DB, then set the max
                     * value
                     * or retrieve the counter number from Mem DB
                     */
                    icb->second->_connections = !counter.has_value()
                              ? std::numeric_limits<std::size_t>::max()
                              : std::stoi(counter.value());

                    if (icb->second->_connections < min_server->second->_connections) {
                              min_server = icb;
                    }
          }

          return std::make_shared<grpc::details::ServerInstanceConf>(
                    min_server->second->_host, min_server->second->_port,
                    min_server->second->_name);
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
