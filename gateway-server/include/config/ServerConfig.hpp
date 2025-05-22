#ifndef _INIREADER_HPP_
#define _INIREADER_HPP_
#include <inicpp.h>
#include <memory>
#include <singleton/singleton.hpp>

struct ServerConfig : public Singleton<ServerConfig> {
  friend class Singleton<ServerConfig>;

public:
  ~ServerConfig() = default;
  unsigned short GateServerPort;

  std::string VerificationServerAddress;

  std::string MySQL_host;
  std::string MySQL_port;
  std::string MySQL_username;
  std::string MySQL_passwd;
  std::string MySQL_database;
  std::size_t MySQL_timeout;

  std::string Redis_ip_addr;
  unsigned short Redis_port;
  std::string Redis_passwd;
  std::size_t Redis_timeout;

  std::string BalanceServiceAddress;
  std::string BalanceServicePort;

private:
  ServerConfig() {
    m_ini.load(CONFIG_HOME "config.ini");
    loadGateServerInfo();
    loadVerificationServerInfo();
    loadMySQLInfo();
    loadRedisInfo();
    loadBalanceServiceInfo();
  }

  void loadGateServerInfo() {
    GateServerPort = m_ini["GateServer"]["port"].as<unsigned short>();
  }
  void loadVerificationServerInfo() {
    VerificationServerAddress =
        m_ini["VerificationServer"]["host"].as<std::string>() + ':' +
        std::to_string(
            m_ini["VerificationServer"]["port"].as<unsigned short>());
  }
  void loadMySQLInfo() {
    MySQL_username = m_ini["MySQL"]["username"].as<std::string>();
    MySQL_passwd = m_ini["MySQL"]["password"].as<std::string>();
    MySQL_database = m_ini["MySQL"]["database"].as<std::string>();
    MySQL_host = m_ini["MySQL"]["host"].as<std::string>();
    MySQL_port = m_ini["MySQL"]["port"].as<std::string>();
    MySQL_timeout = m_ini["MySQL"]["timeout"].as<unsigned long>();
  }
  void loadRedisInfo() {
    Redis_port = m_ini["Redis"]["port"].as<unsigned short>();
    Redis_ip_addr = m_ini["Redis"]["host"].as<std::string>();
    Redis_passwd = m_ini["Redis"]["password"].as<std::string>();
    Redis_timeout = m_ini["Redis"]["timeout"].as<unsigned long>();
  }
  void loadBalanceServiceInfo() {
    BalanceServiceAddress = m_ini["BalanceService"]["host"].as<std::string>();
    BalanceServicePort =
        std::to_string(m_ini["BalanceService"]["port"].as<unsigned short>());
  }

private:
  ini::IniFile m_ini;
};

#endif
