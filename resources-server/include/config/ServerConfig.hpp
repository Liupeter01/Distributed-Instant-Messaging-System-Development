#ifndef _INIREADER_HPP_
#define _INIREADER_HPP_
#include <inicpp.h>
#include <singleton/singleton.hpp>

struct ServerConfig : public Singleton<ServerConfig> {
  friend class Singleton<ServerConfig>;

public:
  std::string outputPath;

  std::string GrpcServerName;
  std::string GrpcServerHost;
  unsigned short GrpcServerPort;

  std::string BalanceServiceAddress;
  std::string BalanceServicePort;

  std::string ResourceServeAddress;
  unsigned short ResourceServerPort;
  std::size_t ResourceQueueSize;
  std::size_t ResourcesMsgLength;

  std::string Redis_ip_addr;
  unsigned short Redis_port;
  std::string Redis_passwd;

  std::string MySQL_host;
  std::string MySQL_port;
  std::string MySQL_username;
  std::string MySQL_passwd;
  std::string MySQL_database;
  std::size_t MySQL_timeout;

  ~ServerConfig() = default;

private:
  ServerConfig() {
    /*init config*/
    m_ini.load(CONFIG_HOME "config.ini");
    loadGrpcServerInfo();
    loadMySQLInfo();
    loadRedisInfo();
    loadBalanceService();
    loadResourcesServer();
    loadOutputPath();
  }

  void loadOutputPath() {
    outputPath = m_ini["Output"]["path"].as<std::string>();
  }

  void loadBalanceService() {
    BalanceServiceAddress = m_ini["BalanceService"]["host"].as<std::string>();
    BalanceServicePort =
        std::to_string(m_ini["BalanceService"]["port"].as<unsigned short>());
  }

  void loadResourcesServer() {
    ResourceServeAddress = m_ini["ResourcesServer"]["host"].as<std::string>();
    ResourceServerPort = m_ini["ResourcesServer"]["port"].as<unsigned short>();
    ResourceQueueSize =
        m_ini["ResourcesServer"]["send_queue_size"].as<unsigned long>();

    ResourcesMsgLength = 
        m_ini["ResourcesServer"]["msg_length"].as<unsigned long>();
  }

  void loadGrpcServerInfo() {
    GrpcServerName = m_ini["gRPCServer"]["server_name"].as<std::string>();
    GrpcServerHost = m_ini["gRPCServer"]["host"].as<std::string>();
    GrpcServerPort = m_ini["gRPCServer"]["port"].as<unsigned short>();
  }

  void loadRedisInfo() {
    Redis_port = m_ini["Redis"]["port"].as<unsigned short>();
    Redis_ip_addr = m_ini["Redis"]["host"].as<std::string>();
    Redis_passwd = m_ini["Redis"]["password"].as<std::string>();
  }

  void loadMySQLInfo() {
    MySQL_username = m_ini["MySQL"]["username"].as<std::string>();
    MySQL_passwd = m_ini["MySQL"]["password"].as<std::string>();
    MySQL_database = m_ini["MySQL"]["database"].as<std::string>();
    MySQL_host = m_ini["MySQL"]["host"].as<std::string>();
    MySQL_port = m_ini["MySQL"]["port"].as<std::string>();
    MySQL_timeout = m_ini["MySQL"]["timeout"].as<unsigned long>();
  }

private:
  ini::IniFile m_ini;
};

#endif
