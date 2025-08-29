#include <boost/asio/ip/tcp.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/row_view.hpp>
#include <boost/mysql/statement.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <sql/MySQLConnectionPool.hpp>
#include <tools/magic_enum.hpp>
#include <tools/tools.hpp>

inline mysql::MySQLConnection::TransactionGuard::TransactionGuard(
    MySQLConnection &conn)
    : m_conn(conn), m_active(true) {
  m_conn.executeCommandOrThrow(MySQLSelection::START_TRANSACTION);
}

inline mysql::MySQLConnection::TransactionGuard::~TransactionGuard() {
  if (m_active) {
    try {
      m_conn.executeCommandOrThrow(MySQLSelection::ROLLBACK_TRANSACTION);
    } catch (...) {
      spdlog::error("Rollback failed in destructor.");
    }
  }
}

inline void mysql::MySQLConnection::TransactionGuard::commit() {
  m_conn.executeCommandOrThrow(MySQLSelection::COMMIT_TRANSACTION);
  m_active = false;
}

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

  bool exception_flag = false;

  try {
    if (select == MySQLSelection::START_TRANSACTION) {
      m_inTransaction = true;
    }
    if (select == MySQLSelection::COMMIT_TRANSACTION ||
        select == MySQLSelection::ROLLBACK_TRANSACTION) {
      m_inTransaction = false;
    }

    [[maybe_unused]] boost::mysql::results result =
        executeCommandOrThrow(select, std::forward<Args>(args)...);

    /*is there any results find?
     * prevent segementation fault
     */
    if (result.rows().begin() == result.rows().end()) {
      return std::nullopt;
    }
    return result;

  } catch (const boost::mysql::error_with_diagnostics &err) {

    /*exception happened!*/
    exception_flag = true;

    spdlog::error(
        "{0}:{1} Operation failed with error code: {2} Server diagnostics: {3}",
        __FILE__, __LINE__, std::to_string(err.code().value()),
        err.get_diagnostics().server_message().data());
  }

  if (exception_flag && m_inTransaction) {

    try {
      executeCommandOrThrow(MySQLSelection::ROLLBACK_TRANSACTION);
    } catch (...) {
      spdlog::warn("Rollback failed unexpectedly.");
    }
    m_inTransaction = false;
  }

  return std::nullopt;
}

template <typename... Args>
boost::mysql::results
mysql::MySQLConnection::executeCommandOrThrow(MySQLSelection select,
                                              Args &&...args) {

  boost::mysql::results result;
  std::string key = m_delegator.get()->m_sql[select];

  if (select != MySQLSelection::HEART_BEAT) {
    spdlog::info("Executing MySQL Query: {}", key);
  }
  if (select == MySQLSelection::START_TRANSACTION ||
      select == MySQLSelection::COMMIT_TRANSACTION ||
      select == MySQLSelection::ROLLBACK_TRANSACTION) {
    conn.execute(key, result);
  } else {
    boost::mysql::statement stmt = conn.prepare_statement(key);
    conn.execute(stmt.bind(std::forward<Args>(args)...), result);
  }
  return result;
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
std::optional<std::unique_ptr<user::UserNameCard>>
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
  return std::make_unique<user::UserNameCard>(
      std::to_string(row.at(0).as_int64()), row.at(1).as_string(),
      usr_op.value(), row.at(2).as_string(), row.at(3).as_string(),
      static_cast<user::Sex>(row.at(4).as_int64()));
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

std::optional<std::vector<std::unique_ptr<user::UserFriendRequest>>>
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

  std::vector<std::unique_ptr<user::UserFriendRequest>> list;
  for (auto ib = result.rows().begin(); ib != result.rows().end(); ib++) {
    std::unique_ptr<user::UserFriendRequest> req(
        std::make_unique<user::UserFriendRequest>(
            std::to_string(ib->at(0).as_int64()), /*src_uuid*/
            std::to_string(dst_uuid),             /*dst_uuid*/
            ib->at(1).as_string(),                /*nickname*/
            ib->at(2).as_string(),                /*msg*/
            ib->at(3).as_string(),                /*avator*/
            ib->at(4).as_string(),                /*user name*/
            ib->at(5).as_string(),                /*description*/
            ib->at(6).as_int64() ? user::Sex::Male : user::Sex::Female /*sex*/
            ));
    list.push_back(std::move(req));
  }
  return list;
}

std::optional<std::vector<std::unique_ptr<user::UserNameCard>>>
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

  std::vector<std::unique_ptr<user::UserNameCard>> list;
  for (auto ib = result.rows().begin(); ib != result.rows().end(); ib++) {
    std::unique_ptr<user::UserNameCard> req(
        std::make_unique<user::UserNameCard>(
            std::to_string(ib->at(0).as_int64()), /*friend_uuid*/
            ib->at(1).as_string(),                /*nickname*/
            ib->at(2).as_string(),                /*avator*/
            ib->at(3).as_string(),                /*user name*/
            ib->at(4).as_string(),                /*description*/
            ib->at(5).as_int64() ? user::Sex::Male : user::Sex::Female /*sex*/
            ));
    list.push_back(std::move(req));
  }
  return list;
}

std::optional<std::vector<std::unique_ptr<chat::ChatThreadMeta>>>
mysql::MySQLConnection::getUserChattingThreadIdx(
    const std::size_t self_uuid, const std::size_t cur_thread_id,
    const std::size_t interval, std::string &next_thread_id, bool &is_EOF) {
  /*init*/
  is_EOF = true;
  next_thread_id = cur_thread_id;

  if (!checkUUID(self_uuid)) {
    spdlog::warn("Invalid Dst UUID!");
    return std::nullopt;
  }

  [[maybe_unused]] auto res =
      executeCommand(MySQLSelection::GET_USER_CHAT_THREADS, self_uuid,
                     self_uuid, cur_thread_id, self_uuid, cur_thread_id,
                     /*we need to test EOF*/ interval + 1);

  /*after execute sql query => no value*/
  if (!res.has_value()) {
    return std::nullopt;
  }

  /*sql execute successfully, but no data retrieved!*/
  boost::mysql::results result = res.value();
  if (!result.rows().size()) {
    return std::nullopt;
  }

  std::vector<std::unique_ptr<chat::ChatThreadMeta>> list;
  for (auto ib = result.rows().begin(); ib != result.rows().end(); ib++) {
    std::string thread_id =
        std::to_string(ib->at(0).as_int64()); // std::string("thread_id")
    std::string type_str = ib->at(3).as_string();

    if (type_str == "GROUP") {

      std::unique_ptr<chat::ChatThreadMeta> req(
          std::make_unique<chat::ChatThreadMeta>(thread_id,
                                                 chat::UserChatType::GROUP));
      list.push_back(std::move(req));
    } else {
      auto user1_uuid = std::to_string(ib->at(1).as_int64());
      auto user2_uuid = std::to_string(ib->at(2).as_int64());
      std::unique_ptr<chat::ChatThreadMeta> req(
          std::make_unique<chat::ChatThreadMeta>(
              thread_id, chat::UserChatType::PRIVATE, user1_uuid, user2_uuid));
      list.push_back(std::move(req));
    }
  }

  // if current list size is more than interval(interval + 1)
  // it means, there are some other items to be retrieved
  // it is not the end
  if (list.size() > interval) {
    is_EOF = false;
    list.pop_back(); // we ignore the last one, because its just for EOF test!
  }

  if (!list.empty()) {
    next_thread_id = list.back()->_thread_id;
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

std::optional<std::string> mysql::MySQLConnection::checkPrivateChatExistance(
    const std::size_t user1_uuid, const std::size_t user2_uuid) {
  auto res = executeCommand(MySQLSelection::CHECK_PRIVATE_CHAT_WITH_LOCK,
                            user1_uuid, user2_uuid);
  if (!res.has_value()) {
    return std::nullopt;
  }

  // thread_id!
  return (*res.value().rows().begin()).at(0).as_string();
}

std::optional<std::string>
mysql::MySQLConnection::createNewPrivateChat(const std::size_t user1_uuid,
                                             const std::size_t user2_uuid) {
  const std::size_t user_one = std::min(user1_uuid, user2_uuid);
  const std::size_t user_two = std::max(user1_uuid, user2_uuid);

  auto res1 = executeCommand(MySQLSelection::USER_UUID_CHECK, user1_uuid);
  auto res2 = executeCommand(MySQLSelection::USER_UUID_CHECK, user2_uuid);

  // Not user uuid found!
  if (!res1.has_value() || !res2.has_value())
    return std::nullopt;

  try {
    TransactionGuard transaction_guard(*this);
    if (auto existing = checkPrivateChatExistance(user_one, user_two);
        existing) {
      transaction_guard.commit();
      return existing.value();
    }

    auto res = executeCommandOrThrow(
        MySQLSelection::CREATE_PRIVATE_GLOBAL_THREAD_INDEX, "PRIVATE");
    std::string thread_id = std::to_string(res.last_insert_id());

    executeCommandOrThrow(MySQLSelection::CREATE_PRIVATE_CHAT_BY_USER_PAIR,
                          thread_id, user_one, user_two);

    transaction_guard.commit();
    return thread_id;
  } catch (const boost::mysql::error_with_diagnostics &err) {
    spdlog::error("createPrivateChat failed: {0}:{1} Operation failed with "
                  "error code: {2} Server diagnostics: {3}",
                  __FILE__, __LINE__, std::to_string(err.code().value()),
                  err.get_diagnostics().server_message().data());

    return std::nullopt;
  }
}

bool mysql::MySQLConnection::createModifyChattingHistoryRecord(
    std::vector<std::shared_ptr<chat::MsgInfo>> &info) {

  bool status = true;
  for (auto &item : info)
    status = status && createModifyChattingHistoryRecord(item);
  return status;
}

bool mysql::MySQLConnection::createModifyChattingHistoryRecord(
    std::shared_ptr<chat::MsgInfo> &info) {

  auto is_rows_afftected = [](const boost::mysql::results &flag) {
    return flag.rows().begin() != flag.rows().end();
  };

  if (info->msg_receiver == info->msg_sender)
    return false;

  auto user1_uuid = std::stoi(info->msg_sender);
  auto user2_uuid = std::stoi(info->msg_receiver);

  const std::size_t user_one = std::min(user1_uuid, user2_uuid);
  const std::size_t user_two = std::max(user1_uuid, user2_uuid);

  auto res1 = executeCommand(MySQLSelection::USER_UUID_CHECK, user1_uuid);
  auto res2 = executeCommand(MySQLSelection::USER_UUID_CHECK, user2_uuid);

  // Not user uuid found!
  if (!res1.has_value() || !res2.has_value())
    return false;

  try {
    TransactionGuard transaction_guard(*this);
    boost::mysql::results flag = executeCommandOrThrow(
        MySQLSelection::CHECK_PRIVATE_CHAT_WITH_LOCK, user_one, user_two);

    if (!is_rows_afftected(flag))
      return false; // No Relavant Info Found Here! ROLLBACK

    /*Store Request->Confirmer Init Chat Info In ChatMsgHistoryBank*/
    flag = executeCommandOrThrow(MySQLSelection::CREATE_MSG_HISTORY_BANK_TUPLE,
                                 /*thread_id = */ info->thread_id,
                                 /*message_status = */ 0,
                                 /*message_sender= */ user1_uuid,
                                 /*message_receiver= */ user2_uuid,
                                 /*message_content = */ info->msg_content);

    if (!flag.affected_rows())
      return false; // No Relavant Info Found Here! ROLLBACK

    // get id from ChatMsgHistoryBank
    std::string message_id = std::to_string(flag.last_insert_id());

    transaction_guard.commit();

    info->setMsgID(message_id);
    return true;
  } catch (const boost::mysql::error_with_diagnostics &err) {
    spdlog::error("createPrivateChat failed: {0}:{1} Operation failed with "
                  "error code: {2} Server diagnostics: {3}",
                  __FILE__, __LINE__, std::to_string(err.code().value()),
                  err.get_diagnostics().server_message().data());

    return false;
  }
}

std::optional<std::vector<std::unique_ptr<chat::MsgInfo>>>
mysql::MySQLConnection::getChattingHistoryRecord(const std::size_t thread_id, 
          const std::size_t msg_id, 
          const std::size_t interval, 
          std::string& next_msg_id, 
          bool& is_EOF)
{
          try {
                    is_EOF = true;
                    next_msg_id = msg_id;

                    std::vector<std::unique_ptr<chat::MsgInfo>> result;

                    auto flags = executeCommandOrThrow(MySQLSelection::GET_USER_CHAT_RECORDS, 
                              thread_id, msg_id, interval + 1);

                    if (!flags.affected_rows())   return std::nullopt;
                    if (flags.rows().empty())   return std::nullopt;

                    for (const auto& tuple : flags.rows()) {
                              auto messag_id = tuple.at(0).as_string();                //message_id
                              auto status =  tuple.at(1).as_int64();                 //message_status
                              auto sender = tuple.at(2).as_string();                //message_sender
                              auto receiver =  tuple.at(3).as_string();                //message_receiver
                              [[maybe_unused]] auto timestamp = tuple.at(4).as_string();
                              auto content = tuple.at(5).as_string();                //message_content

                              result.push_back(std::make_unique<chat::TextMsgInfo>(
                                        std::to_string(thread_id), sender, receiver, content, status, timestamp));
                    }

                    // if current list size is more than interval(interval + 1)
                    // it means, there are some other items to be retrieved
                    // it is not the end
                    if (result.size() > interval) {
                              is_EOF = false;
                              result.pop_back(); // we ignore the last one, because its just for EOF test!
                    }

                    if (!result.empty()) {
                              next_msg_id = result.back()->message_id;
                    }

                    return result;
          }
          catch (const boost::mysql::error_with_diagnostics& err) {
                    spdlog::error("createPrivateChat failed: {0}:{1} Operation failed with "
                              "error code: {2} Server diagnostics: {3}",
                              __FILE__, __LINE__, std::to_string(err.code().value()),
                              err.get_diagnostics().server_message().data());

                    return std::nullopt;
          }
}

/*
 * Confirm mutual friendship between requester and confirmer.
 * This transaction includes:
 * 1. Updating the friending request status to 'confirmed'
 * 2. Inserting mutual friend relations: A ¡ú B (with alias), B ¡ú A (no alias)
 * If any step fails, the entire transaction is rolled back.
 *
 * @param requester_uuid UUID of the user who sent the friend request
 * @param confirmer_uuid UUID of the user who confirmed the friend request
 */
std::optional<std::vector<std::shared_ptr<chat::FriendingConfirmInfo>>>
mysql::MySQLConnection::execFriendConfirmationTransaction(
    const std::size_t requester_uuid, const std::size_t confirmer_uuid) {
  auto is_rows_afftected = [](const boost::mysql::results &flag) {
    return flag.rows().begin() != flag.rows().end();
  };
  const std::size_t user_one = std::min(requester_uuid, confirmer_uuid);
  const std::size_t user_two = std::max(requester_uuid, confirmer_uuid);
  const std::string requester = std::to_string(requester_uuid);
  const std::string confimer = std::to_string(confirmer_uuid);
  std::string confirm_message = fmt::format("We are friends now!");
  std::string message_id{};

  try {
    TransactionGuard transaction_guard(*this);
    std::vector<std::shared_ptr<chat::FriendingConfirmInfo>> res;
    boost::mysql::results flag = executeCommandOrThrow(
        MySQLSelection::CHECK_FRIEND_REQUEST_LIST_WITH_LOCK, requester_uuid,
        confirmer_uuid);

    if (!is_rows_afftected(flag))
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    std::string nickname =
        (*flag.rows().begin()).at(1).as_string(); // requester nickname
    std::string req_message =
        (*flag.rows().begin()).at(2).as_string(); // requester message

    /*Generate Default Message if the message is empty!*/
    req_message =
        req_message.empty()
            ? fmt::format("Hi, I' am {} and looking forward to chat with you!",
                          nickname)
            : req_message;

    /*check if update friending status success!*/
    flag = executeCommandOrThrow(MySQLSelection::UPDATE_FRIEND_REQUEST_STATUS,
                                 1, requester_uuid, confirmer_uuid);

    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    /*
     * update the database, and add biddirectional friend authentication
     * messages It should be a double way friend adding, so create friend
     * relationship should be called twice 1 | src = A | dst = B(authenticator)
     * | alternative_name |
     */
    flag = executeCommandOrThrow(MySQLSelection::CREATE_AUTH_FRIEND_ENTRY,
                                 requester_uuid, confirmer_uuid, nickname);

    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    /*
     * update the database, and add biddirectional friend authentication
     * messages It should be a double way friend adding, so create friend
     * relationship MESSAGE SHOULD BE SENT TO THE SESSION UNDER SRC_UUID 2 | B |
     * A                         | <leave it to blank> |
     */
    flag = executeCommandOrThrow(MySQLSelection::CREATE_AUTH_FRIEND_ENTRY,
                                 confirmer_uuid, requester_uuid, "");
    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    // Create a tuple in GlobalThreadIndexTable
    flag = executeCommandOrThrow(
        MySQLSelection::CREATE_PRIVATE_GLOBAL_THREAD_INDEX, "PRIVATE");
    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    // get id from GlobalThreadIndexTable
    std::string thread_id = std::to_string(flag.last_insert_id());

    // Create a tuple in .PrivateChat
    flag =
        executeCommandOrThrow(MySQLSelection::CREATE_PRIVATE_CHAT_BY_USER_PAIR,
                              thread_id, user_one, user_two);
    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    /*Store Request->Confirmer Init Chat Info In ChatMsgHistoryBank*/
    flag = executeCommandOrThrow(MySQLSelection::CREATE_MSG_HISTORY_BANK_TUPLE,
                                 /*thread_id = */ thread_id,
                                 /*message_status = */ 0,
                                 /*message_sender= */ requester_uuid,
                                 /*message_receiver= */ confirmer_uuid,
                                 /*message_content = */ req_message);

    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    message_id.clear();
    message_id = std::to_string(flag.last_insert_id());

    res.push_back(std::make_shared<chat::FriendingConfirmInfo>(
        chat::MsgType::TEXT, thread_id, message_id, requester, confimer,
        req_message));

    /*Store  Confirmer->Request Init Chat Info In ChatMsgHistoryBank*/
    flag = executeCommandOrThrow(MySQLSelection::CREATE_MSG_HISTORY_BANK_TUPLE,
                                 /*thread_id = */ thread_id,
                                 /*message_status = */ 0,
                                 /*message_sender= */ confirmer_uuid,
                                 /*message_receiver= */ requester_uuid,
                                 /*message_content = */ confirm_message);

    if (!flag.affected_rows())
      return std::nullopt; // No Relavant Info Found Here! ROLLBACK

    message_id.clear();
    message_id = std::to_string(flag.last_insert_id());

    /*COMMIT TRANSACTION!*/
    transaction_guard.commit();

    res.push_back(std::make_shared<chat::FriendingConfirmInfo>(
        chat::MsgType::TEXT, thread_id, message_id, confimer, requester,
        confirm_message));

    return res;
  } catch (const boost::mysql::error_with_diagnostics &err) {
    spdlog::error("createPrivateChat failed: {0}:{1} Operation failed with "
                  "error code: {2} Server diagnostics: {3}",
                  __FILE__, __LINE__, std::to_string(err.code().value()),
                  err.get_diagnostics().server_message().data());

    return std::nullopt; // No Relavant Info Found Here! ROLLBACK
  }
}
