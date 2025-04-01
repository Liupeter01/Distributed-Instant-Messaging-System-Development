#include <boost/asio/ip/tcp.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/statement.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <sql/MySQLConnectionPool.hpp>

mysql::MySQLConnection::MySQLConnection(
    std::string_view username, std::string_view password,
    std::string_view database, std::string_view host, std::string_view port,
    mysql::MySQLConnectionPool *shared) noexcept

    : ctx(IOServicePool::get_instance()->getIOServiceContext()),
      ssl_ctx(boost::asio::ssl::context::tls_client),
      conn(ctx.get_executor(), ssl_ctx),
      last_operation_time(
          std::chrono::steady_clock::now()) /*get operation time*/
      ,
      m_delegator(std::shared_ptr<mysql::MySQLConnectionPool>(
          shared, [](mysql::MySQLConnectionPool *) {})) {
  try {
    // Resolve the hostname to get a collection of endpoints
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
    auto endpoints = resolver.resolve(host, port);

    conn.connect(*endpoints.begin(),
                 boost::mysql::handshake_params(username, password, database));
  } catch (const boost::mysql::error_with_diagnostics &err) {
    // Some errors include additional diagnostics, like server-provided error
    // messages. Security note: diagnostics::server_message may contain
    // user-supplied values (e.g. the field value that caused the error) and is
    // encoded using to the connection's character set (UTF-8 by default). Treat
    // is as untrusted input.
    spdlog::error("MySQL Connect Error: {0}\n Server diagnostics: {1}",
                  err.what(), err.get_diagnostics().server_message().data());

    std::abort();
  }
}

mysql::MySQLConnection::~MySQLConnection() { conn.close(); }

template <typename... Args>
std::optional<boost::mysql::results>
mysql::MySQLConnection::executeCommand(MySQLSelection select, Args &&...args) {
  try {
    boost::mysql::results result;
    std::string key = m_delegator.get()->m_sql[select];
    if (select != MySQLSelection::HEART_BEAT) {
              spdlog::info("Executing MySQL Query: {}", key);
    }
    boost::mysql::statement stmt = conn.prepare_statement(key);
    conn.execute(stmt.bind(std::forward<Args>(args)...), result);

    /*is there any results find?
     * prevent segementation fault
     */
    if (result.rows().begin() == result.rows().end()) {
      return std::nullopt;
    }
    return result;

  } catch (const boost::mysql::error_with_diagnostics &err) {
    spdlog::error(
        "{0}:{1} Operation failed with error code: {2} Server diagnostics: {3}",
        __FILE__, __LINE__, std::to_string(err.code().value()),
        err.get_diagnostics().server_message().data());
    return std::nullopt;
  }
}

std::optional<std::size_t>
mysql::MySQLConnection::checkAccountLogin(std::string_view username,
                                          std::string_view password) {
  auto res =
      executeCommand(MySQLSelection::USER_LOGIN_CHECK, username, password);
  if (!res.has_value()) {
    return std::nullopt;
  }
  boost::mysql::results result = res.value();
  return result.rows().size();
}

bool mysql::MySQLConnection::checkAccountAvailability(std::string_view username,
                                                      std::string_view email) {
  auto res =
      executeCommand(MySQLSelection::FIND_EXISTING_USER, username, email);
  if (!res.has_value()) {
    return false;
  }

  boost::mysql::results result = res.value();
  return result.rows().size();
}

/*get user profile*/
std::optional<std::unique_ptr<UserNameCard>>
mysql::MySQLConnection::getUserProfile(std::size_t uuid) {
  /*get user name by uuid*/
  std::optional<std::string> usr_op = getUsernameByUUID(uuid);
  if (!usr_op.has_value()) {
    return std::nullopt;
  }

  /*get other user profile*/
  auto res = executeCommand(MySQLSelection::USER_PROFILE, uuid);
  if (!res.has_value()) {
    return std::nullopt;
  }

  boost::mysql::results result = res.value();
  boost::mysql::row_view row = *result.rows().begin();
  return std::make_unique<UserNameCard>(
      std::to_string(row.at(0).as_int64()), row.at(1).as_string(),
      usr_op.value(), row.at(2).as_string(), row.at(3).as_string(),
      static_cast<Sex>(row.at(4).as_int64()));
}

bool mysql::MySQLConnection::checkTimeout(
    const std::chrono::steady_clock::time_point &curr, std::size_t timeout) {
  if (std::chrono::duration_cast<std::chrono::seconds>(curr -
                                                       last_operation_time)
          .count() > timeout) {
    return sendHeartBeat();
  }
  return true;
}

bool mysql::MySQLConnection::checkUUID(std::size_t uuid) {
  auto res = executeCommand(MySQLSelection::USER_UUID_CHECK, uuid);
  if (!res.has_value()) {
    return false;
  }

  boost::mysql::results result = res.value();
  return result.rows().size();
}

std::optional<std::size_t>
mysql::MySQLConnection::getUUIDByUsername(std::string_view username) {
  auto res = executeCommand(MySQLSelection::GET_USER_UUID, username);
  if (!res.has_value()) {
    return std::nullopt;
  }

  return (*res.value().rows().begin()).at(0).as_int64();
}

std::optional<std::string>
mysql::MySQLConnection::getUsernameByUUID(std::size_t uuid) {
  auto res = executeCommand(MySQLSelection::USER_UUID_CHECK, uuid);
  if (!res.has_value()) {
    return std::nullopt;
  }
  return (*res.value().rows().begin()).at(1).as_string();
}

bool mysql::MySQLConnection::sendHeartBeat() {
  return executeCommand(MySQLSelection::HEART_BEAT).has_value();
}

void mysql::MySQLConnection::updateTimer() {
  last_operation_time = std::chrono::steady_clock::now();
}
