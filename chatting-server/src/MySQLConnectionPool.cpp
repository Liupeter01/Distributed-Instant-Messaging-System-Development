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
    [[maybe_unused]] bool res =
        connector(username, password, database, host, port);
  }

  m_RRThread = std::thread([this]() {
    thread_local std::size_t counter{0};
    spdlog::info("[HeartBeat Check]: Timeout Setting {}s", m_timeout);

    while (m_stop) {

      if (counter == m_timeout) {
        roundRobinChecking();
        counter = 0; // reset
      }

      /*suspend this thread by timeout setting*/
      std::this_thread::sleep_for(std::chrono::seconds(1));
      counter++;
    }
  });
  m_RRThread.detach();
}

mysql::MySQLConnectionPool::~MySQLConnectionPool() {}

void mysql::MySQLConnectionPool::registerSQLStatement() {
  m_sql.insert(std::pair(MySQLSelection::HEART_BEAT, fmt::format("SELECT 1")));

  /*Transaction Control*/
  m_sql.insert(std::pair(MySQLSelection::START_TRANSACTION,
                         fmt::format("START TRANSACTION")));
  m_sql.insert(
      std::pair(MySQLSelection::COMMIT_TRANSACTION, fmt::format("COMMIT")));
  m_sql.insert(
      std::pair(MySQLSelection::ROLLBACK_TRANSACTION, fmt::format("ROLLBACK")));

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
      MySQLSelection::CHECK_FRIEND_REQUEST_LIST_WITH_LOCK,
      fmt::format("SELECT {}, {}, {} FROM {} "
                  "WHERE {} = ? AND {} = ? "
                  "FOR UPDATE",
                  std::string("id"), std::string("nickname"),
                  std::string("message"), std::string("FriendRequest"),
                  std::string("src_uuid "), std::string("dst_uuid"))));

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

  m_sql.insert(
      std::pair(MySQLSelection::GET_USER_CHAT_THREADS,
                fmt::format("WITH all_threads AS ("
                            "SELECT {0}, {1}, {2}, {6} AS {3} "
                            "FROM {8} "
                            "WHERE ({1} = ? OR {2} = ?) AND {8}.{0} > ? "
                            "UNION ALL "
                            "SELECT {0}, {9} AS {1}, {9} AS {2}, {7} AS {3} "
                            "FROM {10} "
                            "WHERE {4} = ? AND {10}.{0} > ?"
                            ") "
                            "SELECT * FROM all_threads ORDER BY {0}"
                            "  LIMIT ?;",
                            std::string("thread_id"),            // {0}
                            std::string("user1_uuid"),           // {1}
                            std::string("user2_uuid"),           // {2}
                            std::string("type"),                 // {3}
                            std::string("user_uuid"),            // {4}
                            std::string("unused"),               // {5}
                            std::string("'PRIVATE'"),            // {6}
                            std::string("'GROUP'"),              // {7}
                            std::string("chatting.PrivateChat"), // {8}
                            std::string("NULL"),                 // {9}
                            std::string("chatting.GroupMember")  // {10}
                            )));

  m_sql.insert(std::pair(
      MySQLSelection::GET_USER_CHAT_RECORDS,
      fmt::format("SELECT {2}, {3}, {4}, {5}, {6}, {7} "
                  "FROM {0} "
                  " WHERE {1} = ? AND {2} > ? "
                  " ORDER BY {2}  ASC "
                  "LIMIT ? ;",
                  std::string("chatting.ChatMsgHistoryBank"),
                  std::string("thread_id"),
                  std::string("message_id"), // 2
                  std::string("message_status"), std::string("message_sender"),
                  std::string("message_receiver"), std::string("created_at"),
                  std::string("message_content"))));

  m_sql.insert(std::pair(MySQLSelection::CHECK_PRIVATE_CHAT_WITH_LOCK,
                         fmt::format("SELECT {0} FROM {1} "
                                     "WHERE ({2} = ? AND {3} = ?) "
                                     "FOR UPDATE;",
                                     std::string("thread_id"),   // {0}
                                     std::string("PrivateChat"), // {1}
                                     std::string("user1_uuid"),  // {2}
                                     std::string("user2_uuid")   // {3}
                                     )));

  m_sql.insert(std::pair(MySQLSelection::CREATE_PRIVATE_CHAT_BY_USER_PAIR,
                         fmt::format("INSERT INTO {0} ({1}, {2}, {3}, {4}) "
                                     "VALUES (?, ?, ?, NOW());",
                                     std::string("PrivateChat"), // {0}
                                     std::string("thread_id"),   // {1}
                                     std::string("user1_uuid"),  // {2}
                                     std::string("user2_uuid"),  // {3}
                                     std::string("created_at")   // {4}
                                     )));

  m_sql.insert(std::pair(
      MySQLSelection::CREATE_MSG_HISTORY_BANK_TUPLE,
      fmt::format("INSERT INTO {} ({}, {}, {}, {}, {}, {}, {}) "
                  " VALUES (?, ?, ?, ?, NOW(), NOW(), ?);",
                  std::string("ChatMsgHistoryBank"), std::string("thread_id"),
                  std::string("message_status"), std::string("message_sender"),
                  std::string("message_receiver"), std::string("created_at"),
                  std::string("updated_at"), std::string("message_content"))));

  m_sql.insert(
      std::pair(MySQLSelection::CREATE_PRIVATE_GLOBAL_THREAD_INDEX,
                fmt::format("INSERT INTO {0} ({1}, {2}) VALUES (?, NOW());",
                            std::string("GlobalThreadIndexTable"), // {0}
                            std::string("type"),                   // {1}
                            std::string("created_at")              // {2}
                            )));
}

void mysql::MySQLConnectionPool::roundRobinChecking() {
  roundRobinCheckLowGranularity();
}

void mysql::MySQLConnectionPool::roundRobinCheckLowGranularity() {
  if (m_stop)
    return;

  std::size_t fail_count = 0;
  std::size_t expectedStubs{0}, currentStubs{0};
  auto currentTimeStamp = std::chrono::steady_clock::now();

  // get target queue size first, we need to know how many stubs are instead the
  // queue
  {
    std::lock_guard<std::mutex> _lckg(m_mtx);
    expectedStubs = m_stub_queue.size();
  }

  for (; !expectedStubs && currentStubs < expectedStubs; currentStubs++) {

    {
      // sometimes. m_stub_queue might be empty;
      std::lock_guard<std::mutex> _lckg(m_mtx);
      if (m_stub_queue.empty()) {
        break;
      }
    }

    // get stub from the queue
    connection::ConnectionRAII<mysql::MySQLConnectionPool,
                               mysql::MySQLConnection>
        instance;

    if (std::chrono::duration_cast<std::chrono::seconds>(
            currentTimeStamp - instance->get()->last_operation_time) <
        std::chrono::seconds(5)) {
      continue;
    }

    try {
      /*execute timeout checking, if there is sth wrong , then throw exceptionn
       * and re-create connction*/
      if (!instance->get()->checkTimeout(currentTimeStamp, m_timeout))
          [[unlikely]]
        throw std::runtime_error("Check Timeout Failed!");

      /*update current operation time!*/
      instance->get()->last_operation_time = currentTimeStamp;
    } catch (const std::exception &e) {

      /*checktimeout error, but we will handle restart later*/
      spdlog::warn("[MySQL DataBase]: Error = {} Restarting Connection...",
                   e.what());

      // disable RAII feature to return this item back to the pool
      instance.invalidate();

      fail_count++; // record failed time!
    }
  }

  // handle failed events, and try to reconnect
  while (fail_count > 0) {
    if (!connector(m_username, m_password, m_database, m_host, m_port))
        [[unlikely]] {
      return;
    }
    fail_count--;
  }
}

bool mysql::MySQLConnectionPool::connector(const std::string &username,
                                           const std::string &password,
                                           const std::string &database,
                                           const std::string &host,
                                           const std::string &port) {
  auto currentTimeStamp = std::chrono::steady_clock::now();

  try {
    auto new_item = std::make_unique<mysql::MySQLConnection>(
        username, password, database, host, port, this);
    new_item->last_operation_time = currentTimeStamp;

    {
      std::lock_guard<std::mutex> _lckg(m_mtx);
      m_stub_queue.push(std::move(new_item));
    }

    return true;
  } catch (const std::exception &e) {
    spdlog::warn("[MySQL Connector]: Error = {}", e.what());
  }
  return false;
}
