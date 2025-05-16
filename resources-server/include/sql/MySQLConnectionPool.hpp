#pragma once
#ifndef _MYSQLMANAGEMENT_HPP_
#define _MYSQLMANAGEMENT_HPP_
#include <service/ConnectionPool.hpp>
#include <sql/MySQLConnection.hpp>

namespace mysql {
class MySQLConnectionPool
    : public connection::ConnectionPool<MySQLConnectionPool,
                                        mysql::MySQLConnection> {
  using context = mysql::MySQLConnection;
  using context_ptr = std::unique_ptr<mysql::MySQLConnection>;
  friend class Singleton<MySQLConnectionPool>;
  friend class MySQLConnection;

public:
  virtual ~MySQLConnectionPool();

private:
  MySQLConnectionPool() noexcept;
  MySQLConnectionPool(
      std::size_t timeOut, const std::string &username,
      const std::string &password, const std::string &database,
      const std::string &host = "localhost",
      const std::string &port = boost::mysql::default_port_string) noexcept;

  void registerSQLStatement();
  void roundRobinChecking();

private:
  std::string m_username;
  std::string m_password;
  std::string m_database;
  std::string m_host;
  std::string m_port;

  /*round-robin timeout check(second)*/
  std::mutex m_RRMutex;
  std::size_t m_timeout;

  /*round robin thread*/
  std::thread m_RRThread;

  /*sql operation command*/
  std::map<MySQLSelection, std::string> m_sql;
};
} // namespace mysql

#endif // !_MYSQLMANAGEMENT_HPP_
