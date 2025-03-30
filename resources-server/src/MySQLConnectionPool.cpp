#include <config/ServerConfig.hpp>
#include <spdlog/spdlog.h>
#include <sql/MySQLConnectionPool.hpp>

mysql::MySQLConnectionPool::MySQLConnectionPool() noexcept
    : MySQLConnectionPool(ServerConfig::get_instance()->MySQL_timeout,
                          ServerConfig::get_instance()->MySQL_username,
                          ServerConfig::get_instance()->MySQL_passwd,
                          ServerConfig::get_instance()->MySQL_database,
                          ServerConfig::get_instance()->MySQL_host,
                          ServerConfig::get_instance()->MySQL_port) {
  spdlog::info("Connecting to MySQL service ip: {0}, port: {1}, database: {2}",
               ServerConfig::get_instance()->MySQL_username,
               ServerConfig::get_instance()->MySQL_passwd,
               ServerConfig::get_instance()->MySQL_database);
}

mysql::MySQLConnectionPool::MySQLConnectionPool(
    std::size_t timeOut, const std::string &username,
    const std::string &password, const std::string &database,
    const std::string &host, const std::string &port) noexcept
    : m_timeout(timeOut), m_username(username), m_password(password),
      m_database(database), m_host(host), m_port(port) {
  registerSQLStatement();

  for (std::size_t i = 0; i < m_queue_size; ++i) {
    m_stub_queue.push(std::move(std::make_unique<mysql::MySQLConnection>(
        username, password, database, host, port, this)));
  }

  m_RRThread = std::thread([this]() {
    while (!m_stop) {
      spdlog::info("[Chatting Server HeartBeat Check]: Timeout Setting {}s",
                   m_timeout);

      roundRobinChecking();

      /*suspend this thread by timeout setting*/
      std::this_thread::sleep_for(std::chrono::seconds(m_timeout));
    }
  });
  m_RRThread.detach();
}

mysql::MySQLConnectionPool::~MySQLConnectionPool() {}

void mysql::MySQLConnectionPool::registerSQLStatement() {
  m_sql.insert(std::pair(MySQLSelection::HEART_BEAT, fmt::format("SELECT 1")));
  m_sql.insert(std::pair(
      MySQLSelection::FIND_EXISTING_USER,
      fmt::format("SELECT * FROM Authentication WHERE {} = ? AND {} = ?",
                  std::string("username"), std::string("email"))));

  m_sql.insert(std::pair(
      MySQLSelection::CREATE_NEW_USER,
      fmt::format("INSERT INTO Authentication ({},{},{}) VALUES (? ,? ,? )",
                  std::string("username"), std::string("password"),
                  std::string("email"))));

  m_sql.insert(std::pair(
      MySQLSelection::UPDATE_USER_PASSWD,
      fmt::format("UPDATE Authentication SET {} = ? WHERE {} = ? AND {} = ?",
                  std::string("password"), std::string("username"),
                  std::string("email"))));
  m_sql.insert(std::pair(
      MySQLSelection::USER_LOGIN_CHECK,
      fmt::format("SELECT * FROM Authentication WHERE {} = ? AND {} = ?",
                  std::string("username"), std::string("password"))));

  m_sql.insert(
      std::pair(MySQLSelection::USER_UUID_CHECK,
                fmt::format("SELECT * FROM Authentication WHERE {} = ?",
                            std::string("uuid"))));

  m_sql.insert(std::pair(MySQLSelection::USER_PROFILE,
                         fmt::format("SELECT * FROM UserProfile WHERE {} = ?",
                                     std::string("uuid"))));

  m_sql.insert(
      std::pair(MySQLSelection::GET_USER_UUID,
                fmt::format("SELECT uuid FROM Authentication WHERE {} = ?",
                            std::string("username"))));

  m_sql.insert(std::pair(
      MySQLSelection::CREATE_FRIENDING_REQUEST,
      fmt::format(
          "INSERT INTO FriendRequest ({},{},{},{},{}) VALUES (?, ?, ?, ?, 0)"
          " ON DUPLICATE KEY UPDATE src_uuid = src_uuid, dst_uuid = dst_uuid",
          std::string("src_uuid"), std::string("dst_uuid"),
          std::string("nickname"), std::string("message"),
          std::string("status"))));

  m_sql.insert(std::pair(
      MySQLSelection::UPDATE_FRIEND_REQUEST_STATUS,
      fmt::format("UPDATE FriendRequest SET {} = ? WHERE {} = ? AND {} = ?",
                  std::string("status"), std::string("src_uuid"),
                  std::string("dst_uuid"))));

  m_sql.insert(std::pair(
      MySQLSelection::GET_FRIEND_REQUEST_LIST,
      fmt::format(
          "SELECT {}, {}, {}, {}, {}, {}, {} "
          " FROM FriendRequest "
          " JOIN Authentication AS Auth1 ON {} = {} "
          " JOIN Authentication AS Auth2 ON {} = {} "
          " JOIN UserProfile AS UP1 ON {} = {} "
          " JOIN UserProfile AS UP2 ON {} = {} "
          " WHERE {} = ? AND {} = ? AND {} > ? ORDER BY {} ASC LIMIT ? ",
          std::string("FriendRequest.src_uuid"),
          std::string("FriendRequest.nickname"),
          std::string("FriendRequest.message"), std::string("UP1.avatar"),
          std::string("Auth1.username"), std::string("UP1.description"),
          std::string("UP1.sex"),

          std::string("Auth1.uuid"), std::string("FriendRequest.src_uuid"),
          std::string("Auth2.uuid"), std::string("FriendRequest.dst_uuid"),
          std::string("UP1.uuid"), std::string("FriendRequest.src_uuid"),
          std::string("UP2.uuid"), std::string("FriendRequest.dst_uuid"),

          std::string("FriendRequest.status"),
          std::string("FriendRequest.dst_uuid"),
          std::string("FriendRequest.id"), std::string("FriendRequest.id"))));

  m_sql.insert(std::pair(
      MySQLSelection::GET_AUTH_FRIEND_LIST,
      fmt::format("SELECT {}, {}, {}, {}, {}, {}"
                  " FROM AuthFriend AS AF "
                  " JOIN FriendRequest AS FR ON {} = {} "
                  " JOIN Authentication AS Auth1 ON {} = {} "
                  " JOIN Authentication AS Auth2 ON {} = {}"
                  " JOIN UserProfile AS UP1 ON {} = {} "
                  " JOIN UserProfile AS UP2 ON {} = {} "
                  " WHERE {} = ? AND {} = ? AND {} > ? ORDER BY {} ASC LIMIT ?",
                  std::string("AF.friend_uuid"), std::string("FR.nickname"),
                  std::string("UP2.avatar"), std::string("Auth2.username"),
                  std::string("UP2.description"), std::string("UP2.sex"),

                  std::string("AF.self_uuid"), std::string("FR.dst_uuid"),

                  // verify both user's identity
                  std::string("AF.self_uuid"), std::string("Auth1.uuid"),
                  std::string("AF.friend_uuid"), std::string("Auth2.uuid"),

                  // verify both user's identity
                  std::string("AF.self_uuid"), std::string("UP1.uuid"),
                  std::string("AF.friend_uuid"), std::string("UP2.uuid"),

                  std::string("FR.status"), std::string("AF.self_uuid"),
                  std::string("AF.id"), std::string("AF.id"))));

  m_sql.insert(
      std::pair(MySQLSelection::UPDATE_FRIEND_REQUEST_STATUS,
                fmt::format("UPDATE Request SET {} = 1 WHERE {} = ? AND {} = ?",
                            std::string("Request.status"),
                            std::string("Request.src_uuid"),
                            std::string("Request.dst_uuid"))));

  m_sql.insert(std::pair(MySQLSelection::CREATE_AUTH_FRIEND_ENTRY,
                         fmt::format("INSERT IGNORE INTO AuthFriend({}, {}, {})"
                                     "VALUES(?, ?, ?)",
                                     std::string("friend_uuid"),
                                     std::string("self_uuid"),
                                     std::string("alternative_name"))));
}

void mysql::MySQLConnectionPool::roundRobinChecking() {
  std::lock_guard<std::mutex> _lckg(m_RRMutex);
  if (m_stop) {
    return;
  }

  for (std::size_t i = 0; i < m_stub_queue.size(); ++i) {
    connection::ConnectionRAII<mysql::MySQLConnectionPool,
                               mysql::MySQLConnection>
        instance;

    [[maybe_unused]] bool status = instance->get()->checkTimeout(
        std::chrono::steady_clock::now(), m_timeout);

    /*checktimeout error, then create a new connection*/
    if (!status) [[unlikely]] {
      /*create a new connection*/
      m_stub_queue.push(std::move(std::make_unique<mysql::MySQLConnection>(
          m_username, m_password, m_database, m_host, m_port, this)));

      m_cv.notify_one();
    }
  }
}
