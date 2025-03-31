#pragma once
#ifndef _RESOURCESSERVICEPOOL_HPP_
#define _RESOURCESSERVICEPOOL_HPP_
#include <config/ServerConfig.hpp>
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>
#include <service/ConnectionPool.hpp>
#include <spdlog/spdlog.h>

namespace stubpool {
class ResourcesServicePool
    : public connection::ConnectionPool<
          ResourcesServicePool, typename message::ResourceService::Stub> {
  using self = ResourcesServicePool;
  using data_type = typename message::ResourceService::Stub;
  using context = data_type;
  using context_ptr = std::unique_ptr<data_type>;
  friend class Singleton<ResourcesServicePool>;

  grpc::string m_host;
  grpc::string m_port;
  std::shared_ptr<grpc::ChannelCredentials> m_cred;

  ResourcesServicePool()
      : connection::ConnectionPool<self, data_type>(),
        m_host(ServerConfig::get_instance()->BalanceServiceAddress),
        m_port(ServerConfig::get_instance()->BalanceServicePort),
        m_cred(grpc::InsecureChannelCredentials()) {

    auto address = fmt::format("{}:{}", m_host, m_port);
    spdlog::info("Connected to Balance Server {}", address);

    /*creating multiple stub*/
    for (std::size_t i = 0; i < m_queue_size; ++i) {
      m_stub_queue.push(std::move(message::ResourceService::NewStub(
          grpc::CreateChannel(address, m_cred))));
    }
  }

public:
  ~ResourcesServicePool() = default;
};
} // namespace stubpool

#endif
