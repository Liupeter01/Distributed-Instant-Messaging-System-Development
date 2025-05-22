#ifndef _INIREADER_HPP_
#define _INIREADER_HPP_
#include <inicpp.h>
#include <singleton/singleton.hpp>

struct ServerConfig : public Singleton<ServerConfig> {
  friend class Singleton<ServerConfig>;

public:
  std::string GrpcServerName;
  std::string GrpcServerHost;
  unsigned short GrpcServerPort;

  unsigned short ChattingServerPort;
  std::size_t ChattingServerQueueSize;
  std::size_t heart_beat_timeout;

  std::string BalanceServiceAddress;
  std::string BalanceServicePort;

  std::string Redis_ip_addr;
  unsigned short Redis_port;
  std::string Redis_passwd;
  std::size_t Redis_timeout;

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
    loadChattingServiceInfo();
    loadGrpcServerInfo();
    loadBalanceServiceInfo();
    loadMySQLInfo();
    loadRedisInfo();
  }

  void loadRedisInfo() {
    Redis_port = m_ini["Redis"]["port"].as<unsigned short>();
    Redis_ip_addr = m_ini["Redis"]["host"].as<std::string>();
    Redis_passwd = m_ini["Redis"]["password"].as<std::string>();

    Redis_timeout = m_ini["Redis"]["timeout"].as<unsigned long>();
  }

  void loadGrpcServerInfo() {
    GrpcServerName = m_ini["gRPCServer"]["server_name"].as<std::string>();
    GrpcServerHost = m_ini["gRPCServer"]["host"].as<std::string>();
    GrpcServerPort = m_ini["gRPCServer"]["port"].as<unsigned short>();
  }

  void loadChattingServiceInfo() {
    ChattingServerPort = m_ini["ChattingServer"]["port"].as<unsigned short>();
    ChattingServerQueueSize =
        m_ini["ChattingServer"]["send_queue_size"].as<int>();
    heart_beat_timeout =
        m_ini["ChattingServer"]["heart_beat_timeout"].as<int>();
  }

  void loadBalanceServiceInfo() {
    BalanceServiceAddress = m_ini["BalanceService"]["host"].as<std::string>();
    BalanceServicePort =
        std::to_string(m_ini["BalanceService"]["port"].as<unsigned short>());
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
