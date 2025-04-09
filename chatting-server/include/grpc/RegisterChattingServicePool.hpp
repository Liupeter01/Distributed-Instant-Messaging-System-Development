#pragma once
#ifndef _REGISTERCHATTINGSERVICEPOOL_HPP_
#define  _REGISTERCHATTINGSERVICEPOOL_HPP_
#include <config/ServerConfig.hpp>
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>
#include <service/ConnectionPool.hpp>
#include <spdlog/spdlog.h>

namespace stubpool {

 class RegisterChattingServicePool
 		:public connection::ConnectionPool<
 			RegisterChattingServicePool, typename message::ChattingRegisterService::Stub>{
	
	  using self = RegisterChattingServicePool;
	  using data_type = typename message::ChattingRegisterService::Stub;
	  using context = data_type;
	  using context_ptr = std::unique_ptr<data_type>;
	  friend class Singleton<self >;

	    grpc::string m_host;
	  grpc::string m_port;
	  std::shared_ptr<grpc::ChannelCredentials> m_cred;

	  RegisterChattingServicePool()
      : connection::ConnectionPool<self, data_type>(),
        m_host(ServerConfig::get_instance()->BalanceServiceAddress),
        m_port(ServerConfig::get_instance()->BalanceServicePort),
        m_cred(grpc::InsecureChannelCredentials()) {
        
		    auto address = fmt::format("{}:{}", m_host, m_port);
    		spdlog::info("[{}]: RegisterChattingServicePool Connected To Balance Server {}",
              		ServerConfig::get_instance()->GrpcServerName, address);

			 /*creating multiple stub*/
		    for (std::size_t i = 0; i < m_queue_size; ++i) {
		      m_stub_queue.push(std::move(message::ChattingRegisterService::NewStub(
		          grpc::CreateChannel(address, m_cred))));
		    }
	  }

public:
  ~RegisterChattingServicePool() = default;
		
 };

}

#endif // _REGISTERCHATTINGSERVICEPOOL_HPP_