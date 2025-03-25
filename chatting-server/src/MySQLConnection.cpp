#include <boost/asio/ip/tcp.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/statement.hpp>
#include <server/UserFriendRequest.hpp>
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
    spdlog::info("Executing MySQL Query: {}", key);
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

bool mysql::MySQLConnection::createFriendRequest(const std::size_t src_uuid,
                                                 const std::size_t dst_uuid,
                                                 std::string_view nickname,
                                                 std::string_view message) {
  if (src_uuid == dst_uuid)
    return false;

  // check both uuid, are they all valid?
  if (checkUUID(src_uuid) && checkUUID(dst_uuid)) {
    [[maybe_unused]] auto res =
        executeCommand(MySQLSelection::CREATE_FRIENDING_REQUEST, src_uuid,
                       dst_uuid, nickname, message);
    return true;
  }
  return false;
}

/*update user friend request to confirmed status*/
bool mysql::MySQLConnection::updateFriendingStatus(const std::size_t src_uuid,
                                                   const std::size_t dst_uuid) {
  if (src_uuid == dst_uuid)
    return false;

  // check both uuid, are they all valid?
  if (checkUUID(src_uuid) && checkUUID(dst_uuid)) {
    [[maybe_unused]] auto res = executeCommand(
        MySQLSelection::UPDATE_FRIEND_REQUEST_STATUS, 1, src_uuid, dst_uuid);
    return true;
  }
  return false;
}

bool mysql::MySQLConnection::createAuthFriendsRelation(
    const std::size_t self_uuid, const std::size_t friend_uuid,
    const std::string &alternative) {
  if (self_uuid == friend_uuid)
    return false;

  // check both uuid, are they all valid?
  if (checkUUID(self_uuid) && checkUUID(friend_uuid)) {
    [[maybe_unused]] auto res =
        executeCommand(MySQLSelection::CREATE_AUTH_FRIEND_ENTRY, self_uuid,
                       friend_uuid, alternative);

    return true;
  }
  return false;
}

std::optional<std::vector<std::unique_ptr<UserFriendRequest>>>
mysql::MySQLConnection::getFriendingRequestList(const std::size_t dst_uuid,
                                                const std::size_t start_pos,
                                                const std::size_t interval) {
  if (!checkUUID(dst_uuid)) {
    spdlog::warn("Invalid Dst UUID!");
    return std::nullopt;
  }

  [[maybe_unused]] auto res =
      executeCommand(MySQLSelection::GET_FRIEND_REQUEST_LIST, dst_uuid,
                     /*status=*/0, start_pos, interval);

  /*after execute sql query => no value*/
  if (!res.has_value()) {
    return std::nullopt;
  }

  /*sql execute successfully, but no data retrieved!*/
  boost::mysql::results result = res.value();
  if (!result.rows().size()) {
    return std::nullopt;
  }

  std::vector<std::unique_ptr<UserFriendRequest>> list;
  for (auto ib = result.rows().begin(); ib != result.rows().end(); ib++) {
    std::unique_ptr<UserFriendRequest> req(std::make_unique<UserFriendRequest>(
        std::to_string(ib->at(0).as_int64()),          /*src_uuid*/
        std::to_string(dst_uuid),                      /*dst_uuid*/
        ib->at(1).as_string(),                         /*nickname*/
        ib->at(2).as_string(),                         /*msg*/
        ib->at(3).as_string(),                         /*avator*/
        ib->at(4).as_string(),                         /*user name*/
        ib->at(5).as_string(),                         /*description*/
        ib->at(6).as_int64() ? Sex::Male : Sex::Female /*sex*/
        ));
    list.push_back(std::move(req));
  }
  return list;
}

std::optional<std::vector<std::unique_ptr<UserNameCard>>>
mysql::MySQLConnection::getAuthenticFriendsList(const std::size_t self_uuid,
                                                const std::size_t start_pos,
                                                const std::size_t interval) {
  if (!checkUUID(self_uuid)) {
    spdlog::warn("Invalid Dst UUID!");
    return std::nullopt;
  }

  [[maybe_unused]] auto res =
      executeCommand(MySQLSelection::GET_AUTH_FRIEND_LIST, self_uuid,
                     /*status=*/1, start_pos, interval);

  /*after execute sql query => no value*/
  if (!res.has_value()) {
    return std::nullopt;
  }

  /*sql execute successfully, but no data retrieved!*/
  boost::mysql::results result = res.value();
  if (!result.rows().size()) {
    return std::nullopt;
  }

  std::vector<std::unique_ptr<UserNameCard>> list;
  for (auto ib = result.rows().begin(); ib != result.rows().end(); ib++) {
    std::unique_ptr<UserNameCard> req(std::make_unique<UserNameCard>(
        std::to_string(ib->at(0).as_int64()),          /*friend_uuid*/
        ib->at(1).as_string(),                         /*nickname*/
        ib->at(2).as_string(),                         /*avator*/
        ib->at(3).as_string(),                         /*user name*/
        ib->at(4).as_string(),                         /*description*/
        ib->at(5).as_int64() ? Sex::Male : Sex::Female /*sex*/
        ));
    list.push_back(std::move(req));
  }
  return list;
}

bool mysql::MySQLConnection::registerNewUser(MySQLRequestStruct &&request) {
  /*check is there anyone who use this username before*/
  if (!checkAccountAvailability(request.m_username, request.m_email)) {
    [[maybe_unused]] auto res =
        executeCommand(MySQLSelection::CREATE_NEW_USER, request.m_username,
                       request.m_password, request.m_email);
    return true;
  }
  return false;
}

bool mysql::MySQLConnection::alterUserPassword(MySQLRequestStruct &&request) {
  if (!checkAccountAvailability(request.m_username, request.m_email)) {
    return false;
  }

  [[maybe_unused]] auto res =
      executeCommand(MySQLSelection::UPDATE_USER_PASSWD, request.m_password,
                     request.m_username, request.m_email);
  return true;
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
