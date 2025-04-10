#pragma once
#ifndef _GRPCDATALAYER_HPP_
#define _GRPCDATALAYER_HPP_
#include <atomic>
#include <memory>
#include <network/def.hpp>
#include <optional>
#include <singleton/singleton.hpp>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_hash_map.h>
#include <type_traits>

namespace grpc {

class GrpcUserServiceImpl;
class GrpcChattingImpl;
class GrpcResourcesImpl;

namespace details {

// forward declaration
class GrpcDataLayer;

struct ServerInstanceConf {
  ServerInstanceConf(const std::string &host, const std::string &port,
                     const std::string &name);

  std::string _host;
  std::string _port;
  std::string _name;
  std::atomic<std::size_t> _connections = 0; /*add init*/
};

struct GRPCServerConf {
  GRPCServerConf(const std::string &host, const std::string &port,
                 const std::string &name);
  std::string _host;
  std::string _port;
  std::string _name;
};

enum class SERVER_TYPE {
  CHATTING_SERVER_INSTANCE,
  RESOURCES_SERVER_INSTANCE,
  CHATTING_GRPC_SERVER,
  RESOURCES_GRPC_SERVER
};

class GrpcDataLayer : public Singleton<GrpcDataLayer> {
  friend class Singleton<GrpcDataLayer>;
  friend class grpc::GrpcUserServiceImpl;
  friend class grpc::GrpcChattingImpl;
  friend class grpc::GrpcResourcesImpl;

  GrpcDataLayer() = default;

public:
  using InstancesMappingType = tbb::concurrent_hash_map<
      /*server name*/ std::string,
      /*server info*/ std::unique_ptr<grpc::details::ServerInstanceConf>>;

  using GrpcMappingType = tbb::concurrent_hash_map<
      /*server name*/ std::string,
      /*server info*/ std::unique_ptr<grpc::details::GRPCServerConf>>;

  template <typename Container>
  struct is_valid_mapping_type : std::false_type {};

  template <>
  struct is_valid_mapping_type<InstancesMappingType> : std::true_type {};

  template <> struct is_valid_mapping_type<GrpcMappingType> : std::true_type {};

public:
  virtual ~GrpcDataLayer() = default;

protected:
  auto &getChattingServerInstances() { return m_chattingServerInstances; }
  auto &getChattingGRPCServer() { return m_chattingGRPCServer; }
  auto &getResourcesServerInstances() { return m_resourcesServerInstances; }
  auto &getResourcesGRPCServer() { return m_resourcesGRPCServer; }

  bool removeItemFromServer(const std::string &server_name, SERVER_TYPE type);
  bool findItemFromServer(const std::string &server_name, SERVER_TYPE type);

  std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
  serverInstanceLoadBalancer(
      SERVER_TYPE type = SERVER_TYPE::CHATTING_SERVER_INSTANCE);

  void registerUserInfo(const std::size_t uuid, const std::string &tokens);

  /*get user token from Redis*/
  std::optional<std::string> getUserToken(const std::size_t uuid);
  ServiceStatus verifyUserToken(const std::size_t uuid,
                                const std::string &tokens);

private:
  std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
  chattingInstanceLoadBalancer();

  std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
  resourcesInstanceLoadBalancer();

  template <typename Container,
            std::enable_if_t<is_valid_mapping_type<Container>::value, int> = 0>
  bool removeItemFromConcurrentHashMap(Container &container,
                                       const std::string &server_name) {
    typename Container::accessor accessor;
    if (container.find(accessor, server_name)) {
      /*remove the item from the container*/
      container.erase(accessor);
      return true;
    }
    return false;
  }

  template <typename Container,
            std::enable_if_t<is_valid_mapping_type<Container>::value, int> = 0>
  bool findItemFromServer(const Container &container,
                          const std::string &server_name) {

    typename Container::const_accessor accessor;
    return container.find(accessor, server_name);
  }

private:
  /*redis*/
  const std::string redis_server_login = "redis_server";

  /*user token predix*/
  const std::string token_prefix = "user_token_";

  InstancesMappingType m_chattingServerInstances;
  InstancesMappingType m_resourcesServerInstances;

  GrpcMappingType m_chattingGRPCServer;
  GrpcMappingType m_resourcesGRPCServer;
};
} // namespace details
} // namespace grpc

#endif
