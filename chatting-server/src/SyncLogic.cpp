#include <config/ServerConfig.hpp>
#include <grpc/GrpcDistributedChattingService.hpp>
#include <grpc/GrpcRegisterChattingService.hpp>
#include <grpc/GrpcUserService.hpp>
#include <handler/SyncLogic.hpp>
#include <server/AsyncServer.hpp>
#include <chat/ChattingThreadDef.hpp>
#include <spdlog/spdlog.h>

/*redis*/
std::string SyncLogic::redis_server_login = "redis_server";

/*store user base info in redis*/
std::string SyncLogic::user_prefix = "user_info_";

/*store the server name that this user belongs to*/
std::string SyncLogic::server_prefix = "uuid_";

/*store the current session id that this user belongs to*/
std::string SyncLogic::session_prefix = "session_";

SyncLogic::SyncLogic() : m_stop(false) {
  /*register callbacks*/
  registerCallbacks();

  /*start processing thread to process queue*/
  m_working = std::thread(&SyncLogic::processing, this);
}

SyncLogic::~SyncLogic() { shutdown(); }

void SyncLogic::commit(pair recv_node) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  if (m_queue.size() > ServerConfig::get_instance()->ChattingServerQueueSize) {
    spdlog::warn("SyncLogic Queue is full!");
    return;
  }
  m_queue.push(std::move(recv_node));
  m_cv.notify_one();
}

void SyncLogic::shutdown() {
  m_stop = true;
  m_cv.notify_all();

  /*join the working thread*/
  if (m_working.joinable()) {
    m_working.join();
  }
}

/*
 * add user connection counter for current server
 * 1. HGET not exist: Current Chatting server didn't setting up connection
 * counter
 * 2. HGET exist: Increment by 1
 */
void SyncLogic::incrementConnection() {
  RedisRAII raii;

  auto get_distributed_lock =
      raii->get()->acquire(ServerConfig::get_instance()->GrpcServerName, 10, 10,
                           redis::TimeUnit::Milliseconds);

  if (!get_distributed_lock.has_value()) {
    spdlog::error(
        "[{}] Acquire Distributed-Lock In IncrementConnection Failed!",
        ServerConfig::get_instance()->GrpcServerName);
    return;
  }

  spdlog::info(
      "[{}] Acquire Distributed-Lock In  IncrementConnection Successful!",
      ServerConfig::get_instance()->GrpcServerName);

  /*try to acquire value from redis*/
  std::optional<std::string> counter = raii->get()->getValueFromHash(
      redis_server_login, ServerConfig::get_instance()->GrpcServerName);

  std::size_t new_number(0);

  /* redis has this value then read it from redis*/
  if (counter.has_value()) {
    new_number = tools::string_to_value<std::size_t>(counter.value()).value();
  }

  /*incerment and set value to hash by using HSET*/
  if (!raii->get()->setValue2Hash(redis_server_login,
                                  ServerConfig::get_instance()->GrpcServerName,
                                  std::to_string(++new_number))) {

    spdlog::error(
        "[{}] Client Number Can Not Be Written To Redis Cache! Error Occured!",
        ServerConfig::get_instance()->GrpcServerName);
  }

  // release lock
  raii->get()->release(ServerConfig::get_instance()->GrpcServerName,
                       get_distributed_lock.value());

  /*store this user belonged server into redis*/
  spdlog::info("[{}] Now {} Client Has Connected To Current Server",
               ServerConfig::get_instance()->GrpcServerName, new_number);
}

/*
 * check user status current online status, with distributed lock support
 * is this user current on any other server?
 */
std::optional<std::string>
SyncLogic::checkCurrentUser(RedisRAII &raii, const std::string &uuid) {
  return raii->get()->checkValue(server_prefix + uuid);
}

void SyncLogic::kick_session(std::shared_ptr<Session> session) {
  session->sendOfflineMessage();
  session->removeRedisCache(session->get_user_uuid(),
                            session->get_session_id());

  UserManager::get_instance()->moveUserToTerminationZone(
      session->get_user_uuid());
  UserManager::get_instance()->removeUsrSession(session->get_user_uuid());
}

bool SyncLogic::check_and_kick_existing_session(
    std::shared_ptr<Session> session) {
  auto existing = UserManager::get_instance()->getSession(session->s_uuid);
  if (existing.has_value()) {
    spdlog::warn(
        "[{}] Client Session {} UUID {} Has Already Logined On This Server!",
        ServerConfig::get_instance()->GrpcServerName, session->s_session_id,
        session->s_uuid);

    /*kick session
     * session->closeSession(); is not enough!
     */

    kick_session(session);
    return true;
  }
  return false;
}

bool SyncLogic::labelCurrentUser(RedisRAII &raii, const std::string &uuid) {

  const auto key = server_prefix + uuid;

  return raii->get()->delPair(key) &&
         raii->get()->setValue(key,
                               ServerConfig::get_instance()->GrpcServerName);
}

/*store this user belonged session id into redis*/
bool SyncLogic::labelUserSessionID([[maybe_unused]] RedisRAII &raii,
                                   const std::string &uuid,
                                   const std::string &session_id) {

  const auto key = session_prefix + uuid;

  return raii->get()->delPair(key) && raii->get()->setValue(key, session_id);
}

void SyncLogic::updateRedisCache([[maybe_unused]] RedisRAII &raii,
                                 const std::string &uuid,
                                 std::shared_ptr<Session> session) {

  auto uuid_int = std::stoi(uuid);
  auto &new_session = session;
  auto &new_session_id = session->get_session_id();

  /*Distributed-Lock on lock:[uuid], waiting time = 10ms, EX = 10ms*/
  auto get_distributed_lock =
      raii->get()->acquire(uuid, 10, 10, redis::TimeUnit::Milliseconds);

  if (!get_distributed_lock.has_value()) {
    spdlog::error("[{}] UUID = {} Distributed-Lock Acquire Failed!",
                  ServerConfig::get_instance()->GrpcServerName, uuid);
    return;
  }

  spdlog::info("[{}] UUID = {} Acquire Distributed-Lock Successful!",
               ServerConfig::get_instance()->GrpcServerName, uuid);

  /* check user current online status, with distributed lock support */
  if (auto status = checkCurrentUser(raii, uuid)) {
    const auto &current = *status;

    /*this user existing on current server*/
    if (current == ServerConfig::get_instance()->GrpcServerName) {
      /*Get Existing old session object and send offline message then delete it
       * from server*/
      if (auto kick_session = UserManager::get_instance()->getSession(uuid);
          kick_session) {
        auto &old_session = *kick_session;
        old_session->sendOfflineMessage();
        UserManager::get_instance()->moveUserToTerminationZone(
            old_session->get_user_uuid());
        UserManager::get_instance()->removeUsrSession(
            old_session->get_user_uuid(), old_session->get_session_id());
      }
    }
    /*This user  Already Logined On Other Server*/
    else {
      spdlog::info("[{}] UUID = {} Has Already Logined On Other [{}] GRPC "
                   "Server, Executing Distributed Kick Method!",
                   current, uuid, current);

      message::TerminationRequest req;
      req.set_kick_uuid(uuid_int);
      auto response = gRPCDistributedChattingService::get_instance()
                          ->forceTerminateLoginedUser(current, req);

      if (response.error() !=
              static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS) ||
          response.kick_uuid() != uuid_int) {

        spdlog::warn("[{}] Trying to Executing Distributed Kick Method On "
                     "Other [{}] GRPC Server Failed",
                     ServerConfig::get_instance()->GrpcServerName, current);

        generateErrorMessage("Internel Server Error",
                             ServiceType::SERVICE_LOGINRESPONSE,
                             ServiceStatus::LOGIN_UNSUCCESSFUL, session);

        // release lock
        raii->get()->release(uuid, get_distributed_lock.value());
        return;
      }
    }
  }

  /* store this user belonged server & session idinto redis */
  if (!labelCurrentUser(raii, uuid) ||
      !labelUserSessionID(raii, uuid, new_session_id)) {
    spdlog::error("[{}] UUID={} & Session ID={} Can Not Be Written To Redis "
                  "Cache! Error Occured!",
                  ServerConfig::get_instance()->GrpcServerName, uuid,
                  new_session_id);
  }

  /*store this user belonged server into redis*/
  spdlog::info("[{}] UUID={}& Session ID={} Has Written To Redis Cache",
               ServerConfig::get_instance()->GrpcServerName, uuid,
               new_session_id);

  // release lock
  raii->get()->release(uuid, get_distributed_lock.value());
}

/*parse Json*/
bool SyncLogic::parseJson(std::shared_ptr<Session> session, NodePtr &recv,
                          boost::json::object &src_obj) {
  std::optional<std::string> body = recv->get_msg_body();

  if (!body) {
    generateErrorMessage("Failed to parse JSON data",
                         ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return false;
  }

  try {
    src_obj = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {
    generateErrorMessage("Invalid JSON format",
                         ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    return false;
  }
  return true;
}

void SyncLogic::generateErrorMessage(const std::string &log, ServiceType type,
                                     ServiceStatus status, SessionPtr conn) {

  boost::json::object obj;
  obj["error"] = static_cast<uint8_t>(status);
  spdlog::warn(std::string("[{}]: ") + log,
               ServerConfig::get_instance()->GrpcServerName);
  conn->sendMessage(type, boost::json::serialize(obj), conn);
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

/*get user's basic info(name, age, sex, ...) from redis*/
std::optional<std::unique_ptr<user::UserNameCard>>
SyncLogic::getUserBasicInfo(const std::string &key) {

  RedisRAII raii;

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

    return std::make_unique<user::UserNameCard>(
        boost::json::value_to<std::string>(root["uuid"]),
        boost::json::value_to<std::string>(root["avator"]),
        boost::json::value_to<std::string>(root["username"]),
        boost::json::value_to<std::string>(root["nickname"]),
        boost::json::value_to<std::string>(root["description"]),
        static_cast<user::Sex>(root["sex"].as_int64()));

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

    std::unique_ptr<user::UserNameCard> info = std::move(profile_op.value());
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
std::optional<std::vector<std::unique_ptr<user::UserFriendRequest>>>
SyncLogic::getFriendRequestInfo(const std::string &dst_uuid,
                                const std::size_t start_pos,
                                const std::size_t interval) {
  auto uuid_op = tools::string_to_value<std::size_t>(dst_uuid);
  if (!uuid_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    return std::nullopt;
  }

  /*search it in mysql*/
  MySQLRAII mysql;

  //check if we got a valid RAII pointer
  if (auto opt = mysql.get_native(); opt) {
            return (*opt).get()->getFriendingRequestList(uuid_op.value(), start_pos,
                      interval);
  }
  return std::nullopt;
}

/*
 * acquire Friend List
 * get existing authenticated bid-directional friend from database
 * @param: startpos: get friend from the index[startpos]
 * @param: interval: how many friends re going to acquire [startpos, startpos +
 * interval)
 */
std::optional<std::vector<std::unique_ptr<user::UserNameCard>>>
SyncLogic::getAuthFriendsInfo(const std::string &dst_uuid,
                              const std::size_t start_pos,
                              const std::size_t interval) {
  auto uuid_op = tools::string_to_value<std::size_t>(dst_uuid);
  if (!uuid_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    return std::nullopt;
  }

  /*search it in mysql*/
  MySQLRAII mysql;

  //check if we got a valid RAII pointer
  if (auto opt = mysql.get_native(); opt) {
            return (*opt).get()->getAuthenticFriendsList(uuid_op.value(), start_pos,
                      interval);
  }

  return std::nullopt;
}

/*
* acquire ChatThread Info by uuid and an existing thread_id(zero by default)
* @param: cur_thread_id: get record from the index[cur_thread_id + 1]
* @param: interval: how many records are going to be acquired [cur_thread_id + 1, cur_thread_id + 1
* + interval)
*/
std::optional<std::vector<std::unique_ptr<chat::ChatThreadMeta>>>
SyncLogic::getChatThreadInfo(const std::string& self_uuid, 
                                                  const std::size_t cur_thread_id, 
                                                  std::string& next_thread_id, 
                                                  bool& is_EOF, 
                                                  const std::size_t interval)
{
          auto uuid_op = tools::string_to_value<std::size_t>(self_uuid);
          if (!uuid_op.has_value()) {
                    spdlog::warn("Casting string typed key to std::size_t!");
                    return std::nullopt;
          }

          /*search it in mysql*/
          MySQLRAII mysql;

          //check if we got a valid RAII pointer
          if (auto opt = mysql.get_native(); opt) {
                    return (*opt).get()->getUserChattingThreadIdx(uuid_op.value(), 
                              cur_thread_id, 
                              interval,
                              next_thread_id,
                              is_EOF
                    );
          }

          return std::nullopt;
}
