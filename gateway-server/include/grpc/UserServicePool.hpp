#pragma once
#ifndef _USERSERVICEPOOL_HPP_
#define _USERSERVICEPOOL_HPP_
#include <config/ServerConfig.hpp>
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>
#include <service/ConnectionPool.hpp>
#include <spdlog/spdlog.h>

namespace stubpool {
class UserServicePool
    : public connection::ConnectionPool<UserServicePool,
                                        typename message::UserService::Stub> {
  using self = UserServicePool;
  using data_type = typename message::UserService::Stub;
  using context = data_type;
  using context_ptr = std::unique_ptr<data_type>;
  friend class Singleton<UserServicePool>;

  grpc::string m_host;
  grpc::string m_port;
  std::shared_ptr<grpc::ChannelCredentials> m_cred;

  UserServicePool()
      : connection::ConnectionPool<self, data_type>(),
        m_host(ServerConfig::get_instance()->BalanceServiceAddress),
        m_port(ServerConfig::get_instance()->BalanceServicePort),
        m_cred(grpc::InsecureChannelCredentials()) {

    auto address = fmt::format("{}:{}", m_host, m_port);
    spdlog::info("[Gateway Server]: Connected To Balance Server {}", address);

    /*creating multiple stub*/
    for (std::size_t i = 0; i < m_queue_size; ++i) {
      m_stub_queue.push(std::move(
          message::UserService::NewStub(grpc::CreateChannel(address, m_cred))));
    }
  }

public:
  ~UserServicePool() = default;
};
} // namespace stubpool

#endif // !USERSERVICEPOOL
