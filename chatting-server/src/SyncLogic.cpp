#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <config/ServerConfig.hpp>
#include <grpc/GrpcBalanceService.hpp>
#include <grpc/GrpcDistributedChattingService.hpp>
#include <handler/SyncLogic.hpp>
#include <redis/RedisManager.hpp>
#include <server/AsyncServer.hpp>
#include <server/UserFriendRequest.hpp>
#include <server/UserManager.hpp>
#include <spdlog/spdlog.h>
#include <sql/MySQLConnectionPool.hpp>
#include <tools/tools.hpp>

/*redis*/
std::string SyncLogic::redis_server_login = "redis_server";

/*store user base info in redis*/
std::string SyncLogic::user_prefix = "user_info_";

/*store the server name that this user belongs to*/
std::string SyncLogic::server_prefix = "uuid_";

SyncLogic::SyncLogic() : m_stop(false) {
  /*register callbacks*/
  registerCallbacks();

  /*start processing thread to process queue*/
  m_working = std::thread(&SyncLogic::processing, this);
}

SyncLogic::~SyncLogic() { shutdown(); }

void SyncLogic::registerCallbacks() {
  /*
   * ServiceType::SERVICE_LOGINSERVER
   * Handling Login Request
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_LOGINSERVER,
      std::bind(&SyncLogic::handlingLogin, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  /*
   * ServiceType::SERVICE_LOGOUTSERVER
   * Handling Logout Request
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_LOGOUTSERVER,
      std::bind(&SyncLogic::handlingLogout, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  /*
   * ServiceType::SERVICE_SEARCHUSERNAME
   * Handling User Search Username
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_SEARCHUSERNAME,
      std::bind(&SyncLogic::handlingUserSearch, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  /*
   * ServiceType::FRIENDREQUEST_SRC
   * Handling the person who added other(dst) as a friend
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_FRIENDREQUESTSENDER,
      std::bind(&SyncLogic::handlingFriendRequestCreator, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3)));

  /*
   * ServiceType::FRIENDREQUEST_DST
   * Handling the person was being added response to the person who init this
   * action
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_FRIENDREQUESTCONFIRM,
      std::bind(&SyncLogic::handlingFriendRequestConfirm, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3)));

  /*
   * ServiceType::SERVICE_TEXTCHATMSGREQUEST
   * Handling the user send chatting text msg to others
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_TEXTCHATMSGREQUEST,
      std::bind(&SyncLogic::handlingTextChatMsg, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  /*
   * SERVICE_VOICECHATMSGREQUEST
   * Handling the user send chatting voice msg to others
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_VOICECHATMSGREQUEST,
      std::bind(&SyncLogic::handlingVoiceChatMsg, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  /*
   * ServiceType::SERVICE_VIDEOCHATMSGREQUEST
   * Handling the user send chatting video msg to others
   */
  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_VIDEOCHATMSGREQUEST,
      std::bind(&SyncLogic::handlingVideoChatMsg, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));
}

void SyncLogic::commit(pair recv_node) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  if (m_queue.size() > ServerConfig::get_instance()->ChattingServerQueueSize) {
    spdlog::warn("SyncLogic Queue is full!");
    return;
  }
  m_queue.push(std::move(recv_node));
  m_cv.notify_one();
}

/*
 * add user connection counter for current server
 * 1. HGET not exist: Current Chatting server didn't setting up connection
 * counter
 * 2. HGET exist: Increment by 1
 */
void SyncLogic::incrementConnection() {
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*try to acquire value from redis*/
  std::optional<std::string> counter = raii->get()->getValueFromHash(
      redis_server_login, ServerConfig::get_instance()->GrpcServerName);

  std::size_t new_number(0);

  /* redis has this value then read it from redis*/
  if (counter.has_value()) {
    new_number = tools::string_to_value<std::size_t>(counter.value()).value();
  }

  /*incerment and set value to hash by using HSET*/
  raii->get()->setValue2Hash(redis_server_login,
                             ServerConfig::get_instance()->GrpcServerName,
                             std::to_string(++new_number));
}

/*
 *  sub user connection counter for current server
 * 1. HGET not exist: Current Chatting server didn't setting up connection
 * counter
 * 2. HGET exist: Decrement by 1
 */
void SyncLogic::decrementConnection() {
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*try to acquire value from redis*/
  std::optional<std::string> counter = raii->get()->getValueFromHash(
      redis_server_login, ServerConfig::get_instance()->GrpcServerName);

  std::size_t new_number(0);

  /* redis has this value then read it from redis*/
  if (counter.has_value()) {
    new_number = tools::string_to_value<std::size_t>(counter.value()).value();
  }

  /*decerment and set value to hash by using HSET*/
  raii->get()->setValue2Hash(redis_server_login,
                             ServerConfig::get_instance()->GrpcServerName,
                             std::to_string(--new_number));
}

bool SyncLogic::tagCurrentUser(const std::string &uuid) {
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;
  return raii->get()->setValue(server_prefix + uuid,
                               ServerConfig::get_instance()->GrpcServerName);
}

bool SyncLogic::untagCurrentUser(const std::string &uuid) {
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;
  return raii->get()->delPair(server_prefix + uuid);
}

void SyncLogic::generateErrorMessage(const std::string &log, ServiceType type,
                                     ServiceStatus status, SessionPtr conn) {

  boost::json::object obj;
  obj["error"] = static_cast<uint8_t>(status);
  spdlog::error(log);
  conn->sendMessage(type, boost::json::serialize(obj));
}

void SyncLogic::processing() {
  for (;;) {
    std::unique_lock<std::mutex> _lckg(m_mtx);
    m_cv.wait(_lckg, [this]() { return m_stop || !m_queue.empty(); });

    if (m_stop) {
      /*take care of the rest of the tasks, and shutdown synclogic*/
      while (!m_queue.empty()) {
        /*execute callback functions*/
        execute(std::move(m_queue.front()));
        m_queue.pop();
      }
      return;
    }

    auto &front = m_queue.front();
    execute(std::move(m_queue.front()));
    m_queue.pop();
  }
}

void SyncLogic::execute(pair &&node) {
  std::shared_ptr<Session> session = node.first;

  ServiceType type = static_cast<ServiceType>(node.second->_id);
  try {
    /*executing callback on specific type*/
    auto it = m_callbacks.find(type);
    if (it == m_callbacks.end()) {
      spdlog::warn("Service Type Not Found!");
      return;
    }
    m_callbacks[type](type, session, std::move(node.second));
  } catch (const std::exception &e) {
    spdlog::error("Excute Method Failed, Internel Server Error! Error Code {}",
                  e.what());
  }
}

void SyncLogic::handlingLogin(ServiceType srv_type,
                              std::shared_ptr<Session> session, NodePtr recv) {

  boost::json::object src_obj;
  boost::json::object redis_root;

  boost::json::array friendreq;  // pending request
  boost::json::array authfriend; // friends that have already been added

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_obj = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // Parsing failed
  if (!(src_obj.contains("uuid") && src_obj.contains("token"))) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  std::string uuid = boost::json::value_to<std::string>(src_obj["uuid"]);
  std::string token = boost::json::value_to<std::string>(src_obj["token"]);

  spdlog::info("[UUID = {}] Trying to login to ChattingServer with Token {}",
               uuid, token);

  auto uuid_value_op = tools::string_to_value<std::size_t>(uuid);
  if (!uuid_value_op.has_value()) {
    generateErrorMessage("Failed to convert string to number",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  auto response =
      gRPCBalancerService::userLoginToServer(uuid_value_op.value(), token);
  redis_root["error"] = response.error();

  if (response.error() !=
      static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
    spdlog::error("[UUID = {}] Trying to login to ChattingServer Failed!",
                  uuid);
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  /*
   * get user's basic info(name, age, sex, ...) from redis
   * 1. we are going to search for info inside redis first, if nothing found,
   * then goto 2
   * 2. searching for user info inside mysql
   */
  std::optional<std::shared_ptr<UserNameCard>> info_str =
      getUserBasicInfo(uuid);
  if (!info_str.has_value()) {
    spdlog::error("[UUID = {}] Can not find a single user in MySQL and Redis",
                  uuid);
    generateErrorMessage("No User Account Found",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::LOGIN_INFO_ERROR, session);
    return;

  } else {
    /*bind uuid with a session*/
    session->setUUID(uuid);

    /* add user uuid and session as a pair and store it inside usermanager */
    UserManager::get_instance()->alterUserSession(uuid, session);

    /*returning info to client*/
    std::shared_ptr<UserNameCard> info = info_str.value();
    redis_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    redis_root["uuid"] = uuid;
    redis_root["sex"] = static_cast<uint8_t>(info->m_sex);
    redis_root["avator"] = info->m_avatorPath;
    redis_root["username"] = info->m_username;
    redis_root["nickname"] = info->m_nickname;
    redis_root["description"] = info->m_description;

    /*
     * get friend request list from the database
     * The default startpos = 0, interval = 10
     */
    std::optional<std::vector<std::unique_ptr<UserFriendRequest>>>
        requestlist_op = getFriendRequestInfo(uuid);

    if (requestlist_op.has_value()) {
      for (auto &req : requestlist_op.value()) {
        boost::json::object obj;
        obj["src_uuid"] = req->m_uuid;
        obj["dst_uuid"] = req->dst_uuid;
        obj["username"] = req->m_username;
        obj["avator"] = req->m_avatorPath;
        obj["nickname"] = req->m_nickname;
        obj["description"] = req->m_description;
        obj["message"] = req->message;
        obj["sex"] = static_cast<uint8_t>(req->m_sex);
        // redis_root["FriendRequestList"]
        friendreq.push_back(std::move(obj));
      }
    }

    /*acquire Friend List*/
    std::optional<std::vector<std::unique_ptr<UserNameCard>>> friendlist_op =
        getAuthFriendsInfo(uuid);
    if (friendlist_op.has_value()) {
      for (auto &req : friendlist_op.value()) {
        boost::json::object obj;
        obj["uuid"] = req->m_uuid;
        obj["username"] = req->m_username;
        obj["avator"] = req->m_avatorPath;
        obj["nickname"] = req->m_nickname;
        obj["description"] = req->m_description;
        obj["sex"] = static_cast<uint8_t>(req->m_sex);
        // redis_root["AuthFriendList"].append(obj);
        authfriend.push_back(std::move(obj));
      }
    }

    redis_root["FriendRequestList"] = std::move(friendreq);
    redis_root["AuthFriendList"] = std::move(authfriend);

    /*send it back*/
    session->sendMessage(ServiceType::SERVICE_LOGINRESPONSE,
                         boost::json::serialize(redis_root));

    /*
     * add user connection counter for current server
     * 1. HGET not exist: Current Chatting server didn't setting up connection
     * counter
     * 2. HGET exist: Increment by 1
     */
    incrementConnection();

    /*store this user belonged server into redis*/
    if (!tagCurrentUser(uuid)) {
      spdlog::warn("[UUID = {}] Bind Current User To Current Server {}", uuid,
                   ServerConfig::get_instance()->GrpcServerName);
    }
  }
}

void SyncLogic::handlingLogout(ServiceType srv_type,
                               std::shared_ptr<Session> session, NodePtr recv) {

  /*
   * sub user connection counter for current server
   * 1. HGET not exist: Current Chatting server didn't setting up connection
   * counter
   * 2. HGET exist: Decrement by 1
   */
  decrementConnection();

  /*delete user belonged server in redis*/
  if (!untagCurrentUser(session->s_uuid)) {
    spdlog::warn("[UUID = {}] Unbind Current User From Current Server {}",
                 session->s_uuid, ServerConfig::get_instance()->GrpcServerName);
  }
}

void SyncLogic::handlingUserSearch(ServiceType srv_type,
                                   std::shared_ptr<Session> session,
                                   NodePtr recv) {

  boost::json::object src_obj;  /*store json from client*/
  boost::json::object dst_root; /*store json from client*/

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_obj = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // Parsing failed
  if (!src_obj.contains("username")) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  std::string username =
      boost::json::value_to<std::string>(src_obj["username"]);
  spdlog::info("[User UUID = {}] Searching For User {} ", session->s_uuid,
               username);

  /*search username in mysql to get uuid*/
  connection::ConnectionRAII<mysql::MySQLConnectionPool, mysql::MySQLConnection>
      mysql;

  std::optional<std::size_t> uuid_op =
      mysql->get()->getUUIDByUsername(username);

  if (!uuid_op.has_value()) {
    spdlog::warn(
        "[Username = {}] Can not find a single user in MySQL and Redis",
        username);
    generateErrorMessage("No Username Found In DB",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::SEARCHING_USERNAME_NOT_FOUND, session);
    return;
  }

  std::optional<std::unique_ptr<UserNameCard>> card_op =
      getUserBasicInfo(std::to_string(uuid_op.value()));
  /*when user info not found!*/
  if (!card_op.has_value()) {
    spdlog::warn("[UUID = {}] No User Profile Found!", uuid_op.value());
    generateErrorMessage("No User Account Found",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::SEARCHING_USERNAME_NOT_FOUND, session);
    return;
  } else {
    std::unique_ptr<UserNameCard> info = std::move(card_op.value());
    dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    dst_root["uuid"] = info->m_uuid;
    dst_root["sex"] = static_cast<uint8_t>(info->m_sex);
    dst_root["avator"] = info->m_avatorPath;
    dst_root["username"] = info->m_username;
    dst_root["nickname"] = info->m_nickname;
    dst_root["description"] = info->m_description;
  }
  session->sendMessage(ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                       boost::json::serialize(dst_root));
}

/*the person who init friend request*/
void SyncLogic::handlingFriendRequestCreator(ServiceType srv_type,
                                             std::shared_ptr<Session> session,
                                             NodePtr recv) {

  boost::json::object src_root;    /*store json from client*/
  boost::json::object result_root; /*send processing result back to src user*/

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_root = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // Parsing failed
  if (!(src_root.contains("src_uuid") && src_root.contains("dst_uuid"))) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  auto src_uuid =
      boost::json::value_to<std::string>(src_root["src_uuid"]); // my uuid
  auto dst_uuid =
      boost::json::value_to<std::string>(src_root["dst_uuid"]); // target uuid
  auto msg = boost::json::value_to<std::string>(src_root["message"]);
  auto nickname = boost::json::value_to<std::string>(src_root["nickname"]);

  if (src_uuid == dst_uuid) {
    generateErrorMessage("Do Not Add yourself as a friend",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::FRIENDING_YOURSELF, session);

    spdlog::warn("[{}]: Receive UUID = {} Friending itself!",
                 ServerConfig::get_instance()->GrpcServerName, src_uuid);
    return;
  }

  spdlog::info("[{}]: Receive UUID = {}'s Friend Request to UUID = {}",
               ServerConfig::get_instance()->GrpcServerName, src_uuid,
               dst_uuid);

  auto src_uuid_value_op = tools::string_to_value<std::size_t>(src_uuid);
  auto dst_uuid_value_op = tools::string_to_value<std::size_t>(dst_uuid);

  if (!src_uuid_value_op.has_value() || !dst_uuid_value_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    return;
  }

  /*insert friend request info into mysql db*/
  connection::ConnectionRAII<mysql::MySQLConnectionPool, mysql::MySQLConnection>
      mysql;
  if (!mysql->get()->createFriendRequest(src_uuid_value_op.value(),
                                         dst_uuid_value_op.value(), nickname,
                                         msg)) {
    generateErrorMessage(" Insert Friend Request Failed",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::FRIENDING_ERROR, session);

    spdlog::warn("[{} UUID = {}]:  Insert Friend Request Failed",
                 ServerConfig::get_instance()->GrpcServerName, src_uuid);
    return;
  }

  spdlog::info("[{} UUID = {}]:  Insert Friend Request Successful",
               ServerConfig::get_instance()->GrpcServerName, src_uuid);

  /*
   * Search For User Belonged Server Cache in Redis
   * find key = server_prefix + dst_uuid in redis, GET
   */
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;
  std::optional<std::string> server_op =
      raii->get()->checkValue(server_prefix + dst_uuid);

  /*we cannot find it in Redis directly*/
  if (!server_op.has_value()) {
    generateErrorMessage("User Not Found In Any Server",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                         session);
    return;
  }

  /*We have to get src_uuid info on current server */
  std::optional<std::shared_ptr<UserNameCard>> info_str =
      getUserBasicInfo(src_uuid);
  if (!info_str.has_value()) {
    generateErrorMessage("Current UserProfile Load Error!",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                         session);
    return;
  }

  std::shared_ptr<UserNameCard> src_namecard = info_str.value();

  /*Is target user(dst_uuid) and current user(src_uuid) on the same server*/
  if (server_op.value() == ServerConfig::get_instance()->GrpcServerName) {
    /*try to find this target user on current chatting-server*/
    auto session_op = UserManager::get_instance()->getSession(dst_uuid);
    if (!session_op.has_value()) {
      generateErrorMessage("Target User's Session Not Found",
                           ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                           ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                           session);
      return;
    }
    /*send it to dst user*/
    boost::json::object dst_root;

    dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    dst_root["src_uuid"] = src_uuid;
    dst_root["dst_uuid"] = dst_uuid;
    dst_root["src_nickname"] = nickname;
    dst_root["src_message"] = msg;
    dst_root["src_avator"] = src_namecard->m_avatorPath;
    dst_root["src_username"] = src_namecard->m_username;
    dst_root["src_desc"] = src_namecard->m_description;
    dst_root["src_sex"] = static_cast<uint8_t>(src_namecard->m_sex);

    /*propagate the message to dst user*/
    session_op.value()->sendMessage(
        ServiceType::SERVICE_FRIENDREINCOMINGREQUEST,
        boost::json::serialize(dst_root));

    result_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
  } else {
    /*
     * GRPC REQUEST
     * dst_uuid and src_uuid are not on the same server-
     * Pass current user info to other chatting-server
     * by using grpc protocol
     */
    message::FriendRequest grpc_request;
    grpc_request.set_src_uuid(src_uuid_value_op.value());
    grpc_request.set_dst_uuid(dst_uuid_value_op.value());
    grpc_request.set_nick_name(nickname);
    grpc_request.set_req_msg(msg);
    grpc_request.set_avator_path(src_namecard->m_avatorPath);
    grpc_request.set_username(src_namecard->m_username);
    grpc_request.set_description(src_namecard->m_description);
    grpc_request.set_sex(static_cast<uint8_t>(src_namecard->m_sex));

    auto response =
        gRPCDistributedChattingService::get_instance()->sendFriendRequest(
            server_op.value(), grpc_request);
    if (response.error() !=
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::warn("[GRPC {} Service]: UUID = {} Send Request To GRPC {} "
                   "Service Failed!",
                   ServerConfig::get_instance()->GrpcServerName, src_uuid,
                   server_op.value());
      generateErrorMessage("Internel Server Error",
                           ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                           ServiceStatus::FRIENDING_ERROR, session);
      return;
    }

    result_root["error"] = response.error();
  }

  /*send service result back to request sender*/
  result_root["src_uuid"] = src_uuid;
  result_root["dst_uuid"] = dst_uuid;
  session->sendMessage(ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                       boost::json::serialize(result_root));
}

/*the person who receive friend request are going to confirm it*/
void SyncLogic::handlingFriendRequestConfirm(ServiceType srv_type,
                                             std::shared_ptr<Session> session,
                                             NodePtr recv) {

  boost::json::object src_root;    /*store json from client*/
  boost::json::object result_root; /*send processing result back to src user*/

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_root = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // Parsing failed
  if (!(src_root.contains("src_uuid") && src_root.contains("dst_uuid"))) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  /*------------------------target user's uuid-------------------------*/
  auto src_uuid = boost::json::value_to<std::string>(src_root["src_uuid"]);
  auto src_uuid_op = tools::string_to_value<std::size_t>(src_uuid);
  /*----------------------------my uuid-------------------------------*/
  auto dst_uuid = boost::json::value_to<std::string>(src_root["dst_uuid"]);
  auto dst_uuid_op = tools::string_to_value<std::size_t>(dst_uuid);

  auto alternative =
      boost::json::value_to<std::string>(src_root["alternative_name"]);

  if (!src_uuid_op.has_value() || !dst_uuid_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                         ServiceStatus::FRIENDING_ERROR, session);
    return;
  }

  /*connection pool RAII*/
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  connection::ConnectionRAII<mysql::MySQLConnectionPool, mysql::MySQLConnection>
      mysql;

  /*check if update friending status success!*/
  if (!mysql->get()->updateFriendingStatus(src_uuid_op.value(),
                                           dst_uuid_op.value())) {
    spdlog::warn("[FriendRequestConfirm]: UUID = {} Authenticate Friend UUID = "
                 "{} As Friend Failed",
                 src_uuid, dst_uuid);
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                         ServiceStatus::FRIENDING_ERROR, session);
    return;
  }

  spdlog::info("[FriendRequestConfirm]: UUID = {} Authenticate Friend UUID = "
               "{} As Friend Successful",
               src_uuid, dst_uuid);

  /*
   * Response SERVICE_SUCCESS to the authenticator
   * Current session should receive a successful response first
   */
  result_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
  session->sendMessage(ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                       boost::json::serialize(result_root));

  /*
   * update the database, and add biddirectional friend authentication messages
   * It should be a double way friend adding, so create friend relationship
   * should be called twice 1 | src = A | dst = B(authenticator) |
   * alternative_name |
   */
  if (!mysql->get()->createAuthFriendsRelation(
          src_uuid_op.value(), dst_uuid_op.value(), alternative)) {
    spdlog::warn("[FriendRequestConfirm]: Create Friend Network <UUID:{} >>> "
                 "UUID:{}> Successful!",
                 src_uuid, dst_uuid);
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                         ServiceStatus::FRIENDING_ERROR, session);
    return;
  }
  spdlog::info("[FriendRequestConfirm]: Create Friend Network <UUID:{} >>> "
               "UUID:{}> Failed!",
               src_uuid, dst_uuid);

  /*We have to get src user info(src_uuid) on current server */
  std::optional<std::shared_ptr<UserNameCard>> info_str =
      getUserBasicInfo(src_uuid);
  if (!info_str.has_value()) {
    generateErrorMessage("Current UserProfile Load Error!",
                         ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                         ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                         session);
    return;
  }

  boost::json::object root;

  std::shared_ptr<UserNameCard> src_namecard = info_str.value();

  root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
  root["friend_uuid"] = src_uuid;
  root["friend_nickname"] = src_namecard->m_nickname;
  root["friend_avator"] = src_namecard->m_avatorPath;
  root["friend_username"] = src_namecard->m_username;
  root["friend_desc"] = src_namecard->m_description;
  root["friend_sex"] = static_cast<uint8_t>(src_namecard->m_sex);

  session->sendMessage(ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                       boost::json::serialize(root));

  /*
   * update the database, and add biddirectional friend authentication messages
   * It should be a double way friend adding, so create friend relationship
   * MESSAGE SHOULD BE SENT TO THE SESSION UNDER SRC_UUID
   * 2 | B | A                         | <leave it to blank> |
   */
  if (!mysql->get()->createAuthFriendsRelation(dst_uuid_op.value(),
                                               src_uuid_op.value(), "")) {
    spdlog::warn("[FriendRequestConfirm]: Create Friend Network <UUID:{} >>> "
                 "UUID:{}> Successful",
                 dst_uuid, src_uuid);
    return;
  }
  spdlog::info("[FriendRequestConfirm]: Create Friend Network <UUID:{} >>> "
               "UUID:{}> Failed",
               dst_uuid, src_uuid);

  /*
   * Search For User Belonged Server Cache in Redis
   * find key = server_prefix + src_uuid in redis, GET
   */
  std::optional<std::string> server_op =
      raii->get()->checkValue(server_prefix + src_uuid);

  /*we cannot find it in Redis directly*/
  if (!server_op.has_value()) {
    return;
  }

  /*We have to get dst user info(dst_uuid) on current server */
  std::optional<std::shared_ptr<UserNameCard>> dst_info =
      getUserBasicInfo(dst_uuid);

  /*Is target user(src_uuid) and current user(dst_uuid) on the same server*/
  if (server_op.value() == ServerConfig::get_instance()->GrpcServerName) {
    /*try to find this target user on current chatting-server*/
    auto session_op = UserManager::get_instance()->getSession(src_uuid);
    if (!session_op.has_value()) {
      return;
    }

    if (!info_str.has_value()) {
      generateErrorMessage("Current UserProfile Load Error!",
                           ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                           ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                           session_op.value());
      return;
    }

    boost::json::object root;
    std::shared_ptr<UserNameCard> dst_namecard = dst_info.value();

    root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    root["friend_uuid"] = dst_uuid;
    root["friend_nickname"] = dst_namecard->m_nickname;
    root["friend_avator"] = dst_namecard->m_avatorPath;
    root["friend_username"] = dst_namecard->m_username;
    root["friend_desc"] = dst_namecard->m_description;
    root["friend_sex"] = static_cast<uint8_t>(dst_namecard->m_sex);

    /*propagate the message to dst user*/
    session_op.value()->sendMessage(
        ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
        boost::json::serialize(root));
  } else {
    /*
     * GRPC REQUEST
     * dst_uuid and src_uuid are not on the same server
     * Pass current user info to other chatting-server
     * by using grpc protocol
     */

    if (!info_str.has_value()) {
      return;
    }
    std::shared_ptr<UserNameCard> dst_namecard = info_str.value();

    message::FriendRequest grpc_request;
    grpc_request.set_src_uuid(
        src_uuid_op
            .value()); // src is the session number we are going to looking for
    grpc_request.set_dst_uuid(
        dst_uuid_op.value()); // dst is the friend, we are going to transfer
                              // dst's info to grpc
    grpc_request.set_nick_name(dst_namecard->m_nickname);
    grpc_request.set_avator_path(dst_namecard->m_avatorPath);
    grpc_request.set_username(dst_namecard->m_username);
    grpc_request.set_description(dst_namecard->m_description);
    grpc_request.set_sex(static_cast<uint8_t>(dst_namecard->m_sex));

    auto response =
        gRPCDistributedChattingService::get_instance()->confirmFriendRequest(
            server_op.value(), grpc_request);

    if (response.error() !=
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::warn("[GRPC {} Service]: UUID = {} Send Request To GRPC {} "
                   "Service Failed!",
                   ServerConfig::get_instance()->GrpcServerName, src_uuid,
                   server_op.value());
      return;
    }
  }
}

/*Handling the user send chatting text msg to others*/
void SyncLogic::handlingTextChatMsg(ServiceType srv_type,
                                    std::shared_ptr<Session> session,
                                    NodePtr recv) {

  boost::json::object src_root; /*store json from client*/

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_root = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // Parsing failed
  if (!(src_root.contains("text_sender") &&
        src_root.contains("text_receiver"))) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

  auto text_sender =
      boost::json::value_to<std::string>(src_root["text_sender"]); // my uuid
  auto text_receiver = boost::json::value_to<std::string>(
      src_root["text_receiver"]); // target user's uuid
  const auto textMsgArray = src_root["text_msg"];

  auto src_uuid_op = tools::string_to_value<std::size_t>(text_sender);
  auto dst_uuid_op = tools::string_to_value<std::size_t>(text_receiver);

  if (!src_uuid_op.has_value() || !dst_uuid_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                         ServiceStatus::NETWORK_ERROR, session);
    return;
  }

  /*connection pool RAII*/
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;
  connection::ConnectionRAII<mysql::MySQLConnectionPool, mysql::MySQLConnection>
      mysql;

  /*
   * Search For User Belonged Server Cache in Redis
   * find key = server_prefix + text_sender  in redis, GET
   */
  std::optional<std::string> server_op =
      raii->get()->checkValue(server_prefix + text_sender);

  /*we cannot find it in Redis directly*/
  if (!server_op.has_value()) {
    return;
  }

  message::ChattingTextMsgResponse response;
  response.set_error(static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS));

  /*Is target user and msg text sender on the same server*/
  if (server_op.value() == ServerConfig::get_instance()->GrpcServerName) {

    /*try to find this target user on current chatting-server*/
    auto session_op = UserManager::get_instance()->getSession(text_receiver);
    if (!session_op.has_value()) {
      generateErrorMessage("Target User's Session Not Found",
                           ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                           ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                           session);
      return;
    }

    boost::json::object
        dst_root; /*try to do message forwarding to dst target user*/
    dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    dst_root["text_sender"] = text_sender;
    dst_root["text_receiver"] = text_receiver;
    dst_root["text_msg"] = textMsgArray;

    /*propagate the message to dst user*/
    session_op.value()->sendMessage(
        ServiceType::SERVICE_TEXTCHATMSGICOMINGREQUEST,
        boost::json::serialize(dst_root));
  } else {
    message::ChattingTextMsgRequest req;
    req.set_src_uuid(src_uuid_op.value());
    req.set_dst_uuid(dst_uuid_op.value());

    /*generate a grpc repreated message array*/
    auto msgArray = textMsgArray.as_array();
    for (auto &msg : msgArray) {
      if (!msg.is_object()) {
        spdlog::warn("Element in 'text_msg' is not an object.");
        continue;
      }

      auto &obj = msg.as_object();
      if (!(obj.contains("msg_sender") && obj.contains("msg_receiver"))) {
        spdlog::warn("No Sender And Msg Receiver's info!");
        continue;
      }

      message::ChattingHistoryData *data_item = req.add_lists();

      /*msg sender and msg receiver identity*/
      data_item->set_msg_sender(
          boost::json::value_to<std::string>(obj["msg_sender"]));
      data_item->set_msg_receiver(
          boost::json::value_to<std::string>(obj["msg_receiver"]));

      /*generate an unique uuid for this message*/
      data_item->set_msg_id(boost::json::value_to<std::string>(obj["msg_id"]));

      /*send message*/
      data_item->set_msg_content(
          boost::json::value_to<std::string>(obj["msg_content"]));
    }

    response =
        gRPCDistributedChattingService::get_instance()->sendChattingTextMsg(
            server_op.value(), req);

    if (response.error() !=
        static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
      spdlog::warn("[GRPC {} Service]: UUID = {} Send Request To GRPC {} "
                   "Service Failed!",
                   ServerConfig::get_instance()->GrpcServerName, text_receiver,
                   server_op.value());
    }
  }

  /*
   * Response SERVICE_SUCCESS to the text msg sender
   * Current session should receive a successful response first
   */
  boost::json::object result_root; // reponse status to sender
  result_root["error"] = response.error();
  result_root["text_sender"] = text_sender;     // my uuid
  result_root["text_receiver"] = text_receiver; // target user's uuid

  session->sendMessage(ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                       boost::json::serialize(result_root));
}

/*Handling the user send chatting text msg to others*/
void SyncLogic::handlingVoiceChatMsg(ServiceType srv_type,
                                     std::shared_ptr<Session> session,
                                     NodePtr recv) {

  boost::json::object src_root;    /*store json from client*/
  boost::json::object result_root; /*send processing result back to dst user*/

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_root = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }
}

/*Handling the user send chatting text msg to others*/
void SyncLogic::handlingVideoChatMsg(ServiceType srv_type,
                                     std::shared_ptr<Session> session,
                                     NodePtr recv) {
  boost::json::object src_root;    /*store json from client*/
  boost::json::object result_root; /*send processing result back to dst user*/

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }

  // prevent parse error
  try {
    src_root = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return;
  }
}

/*get user's basic info(name, age, sex, ...) from redis*/
std::optional<std::unique_ptr<UserNameCard>>
SyncLogic::getUserBasicInfo(const std::string &key) {
  connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
      raii;

  /*
   * Search For Info Cache in Redis
   * find key = user_prefix  + uuid in redis, GET
   */
  std::optional<std::string> info_str =
      raii->get()->checkValue(user_prefix + key);

  /*we could find it in Redis directly*/
  if (info_str.has_value()) {

    /*parse cache data inside Redis*/
    boost::json::object root;
    try {
      root = boost::json::parse(info_str.value()).as_object();
    } catch (const boost::json::system_error &e) {
      spdlog::error("Failed to parse json data!");
      return std::nullopt;
    }

    return std::make_unique<UserNameCard>(
        boost::json::value_to<std::string>(root["uuid"]),
        boost::json::value_to<std::string>(root["avator"]),
        boost::json::value_to<std::string>(root["username"]),
        boost::json::value_to<std::string>(root["nickname"]),
        boost::json::value_to<std::string>(root["description"]),
        static_cast<Sex>(root["sex"].as_int64()));

  } else {
    boost::json::object redis_root;

    /*search it in mysql*/
    connection::ConnectionRAII<mysql::MySQLConnectionPool,
                               mysql::MySQLConnection>
        mysql;

    auto uuid_op = tools::string_to_value<std::size_t>(key);

    if (!uuid_op.has_value()) {
      spdlog::error("Casting string typed key to std::size_t!");
      return std::nullopt;
    }

    std::size_t uuid = uuid_op.value();
    auto profile_op = mysql->get()->getUserProfile(uuid);

    /*when user info not found!*/
    if (!profile_op.has_value()) {
      spdlog::warn("[UUID = {}] No User Profile Found!", uuid);
      return std::nullopt;
    }

    std::unique_ptr<UserNameCard> info = std::move(profile_op.value());
    redis_root["uuid"] = info->m_uuid;
    redis_root["sex"] = static_cast<uint8_t>(info->m_sex);
    redis_root["avator"] = info->m_avatorPath;
    redis_root["username"] = info->m_username;
    redis_root["nickname"] = info->m_nickname;
    redis_root["description"] = info->m_description;

    /*write data into redis as cache*/
    if (!raii->get()->setValue(user_prefix + key,
                               boost::json::serialize(redis_root))) {
      spdlog::error("[UUID = {}] Write Data To Redis Failed!", uuid);
      return std::nullopt;
    }
    return info;
  }
  return std::nullopt;
}

/*
 * get friend request list from the database
 * @param: startpos: get friend request from the index[startpos]
 * @param: interval: how many requests are going to acquire [startpos, startpos
 * + interval)
 */
std::optional<std::vector<std::unique_ptr<UserFriendRequest>>>
SyncLogic::getFriendRequestInfo(const std::string &dst_uuid,
                                const std::size_t start_pos,
                                const std::size_t interval) {
  auto uuid_op = tools::string_to_value<std::size_t>(dst_uuid);
  if (!uuid_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    return std::nullopt;
  }

  /*search it in mysql*/
  connection::ConnectionRAII<mysql::MySQLConnectionPool, mysql::MySQLConnection>
      mysql;

  return mysql->get()->getFriendingRequestList(uuid_op.value(), start_pos,
                                               interval);
}

/*
 * acquire Friend List
 * get existing authenticated bid-directional friend from database
 * @param: startpos: get friend from the index[startpos]
 * @param: interval: how many friends re going to acquire [startpos, startpos +
 * interval)
 */
std::optional<std::vector<std::unique_ptr<UserNameCard>>>
SyncLogic::getAuthFriendsInfo(const std::string &dst_uuid,
                              const std::size_t start_pos,
                              const std::size_t interval) {
  auto uuid_op = tools::string_to_value<std::size_t>(dst_uuid);
  if (!uuid_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    return std::nullopt;
  }

  /*search it in mysql*/
  connection::ConnectionRAII<mysql::MySQLConnectionPool, mysql::MySQLConnection>
      mysql;

  return mysql->get()->getAuthenticFriendsList(uuid_op.value(), start_pos,
                                               interval);
}

void SyncLogic::shutdown() {
  m_stop = true;
  m_cv.notify_all();

  /*join the working thread*/
  if (m_working.joinable()) {
    m_working.join();
  }
}
