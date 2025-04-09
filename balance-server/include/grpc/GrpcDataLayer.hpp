#pragma once
#ifndef _GRPCDATALAYER_HPP_
#define _GRPCDATALAYER_HPP_
#include <memory>
#include <atomic>
#include <network/def.hpp>
#include <optional>
#include <tbb/concurrent_unordered_map.h>
#include <singleton/singleton.hpp>

namespace grpc {

          class GrpcUserServiceImpl;

          namespace details {

                    struct ServerInstanceConf {
                              ServerInstanceConf(const std::string& host, const std::string& port,
                                        const std::string& name);

                              std::string _host;
                              std::string _port;
                              std::string _name;
                              std::atomic<std::size_t> _connections = 0; /*add init*/
                    };

                    struct GRPCServerConf {
                              GRPCServerConf(const std::string& host, const std::string& port,
                                        const std::string& name);
                              std::string _host;
                              std::string _port;
                              std::string _name;
                    };

                    enum class SERVER_TYPE {
                              CHATTING_SERVER,
                              RESOURCES_SERVER
                    };

                    class GrpcDataLayer :public Singleton<GrpcDataLayer>{
                              friend class Singleton<GrpcDataLayer>;
                              friend class grpc::GrpcUserServiceImpl;
                              GrpcDataLayer() = default;

                    public:
                              virtual ~GrpcDataLayer() = default;

                    protected:
                              std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>> 
                               serverInstanceLoadBalancer(SERVER_TYPE type = SERVER_TYPE::CHATTING_SERVER);

                              void registerUserInfo(const std::size_t uuid, const std::string& tokens);

                              /*get user token from Redis*/
                              std::optional<std::string> getUserToken(const std::size_t uuid);
                              ServiceStatus verifyUserToken(const std::size_t uuid, const std::string& tokens);

                    private:
                              std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
                              chattingInstanceLoadBalancer();

                              std::optional<std::shared_ptr<grpc::details::ServerInstanceConf>>
                               resourcesInstanceLoadBalancer();

                    private:
                              /*redis*/
                              const std::string redis_server_login = "redis_server";

                              /*user token predix*/
                              const std::string token_prefix = "user_token_";

                              
                              tbb::concurrent_unordered_map<
                                        /*server name*/ std::string,
                                        /*server info*/ std::unique_ptr< grpc::details::ServerInstanceConf>>
                                        m_chattingServerInstances;

                              tbb::concurrent_unordered_map<
                                        /*server name*/ std::string,
                                        /*server info*/ std::unique_ptr< grpc::details::GRPCServerConf>>
                                        m_chattingGRPCServer;

                              tbb::concurrent_unordered_map<
                                        /*server name*/ std::string,
                                        /*server info*/ std::unique_ptr< grpc::details::ServerInstanceConf>>
                                        m_resourcesServerInstances;

                              tbb::concurrent_unordered_map<
                                        /*server name*/ std::string,
                                        /*server info*/ std::unique_ptr<  grpc::details::GRPCServerConf>>
                                        m_resourcesGRPCServer;
                    };
          }
}

#endif