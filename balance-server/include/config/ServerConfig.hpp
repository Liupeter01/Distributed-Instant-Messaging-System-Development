#ifndef _INIREADER_HPP_
#define _INIREADER_HPP_
#include <inicpp.h>
#include <memory>
#include <singleton/singleton.hpp>
#include <spdlog/spdlog.h>
#include <vector>

struct ChattingServer {
  std::string _host;
  std::string _port;
  std::string _name;
  std::size_t _connections;
};

struct ServerConfig : public Singleton<ServerConfig> {
  friend class Singleton<ServerConfig>;

public:
  ~ServerConfig() = default;

  std::string Redis_ip_addr;
  unsigned short Redis_port;
  std::string Redis_passwd;
  std::size_t Redis_timeout;

  std::string BalanceServiceAddress;
  std::string BalanceServicePort;

private:
  ServerConfig() {
    /*init config*/
    m_ini.load(CONFIG_HOME "config.ini");
    loadRedisInfo();
    loadBalanceServiceInfo();
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
