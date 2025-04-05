#include <absl/strings/escaping.h> /*base64*/
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <config/ServerConfig.hpp>
#include <filesystem>
#include <fstream>
#include <handler/SyncLogic.hpp>
#include <redis/RedisManager.hpp>
#include <server/AsyncServer.hpp>
#include <server/UserNameCard.hpp>
#include <service/ConnectionPool.hpp>
#include <spdlog/spdlog.h>
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

  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_LOGINSERVER,
      std::bind(&SyncLogic::handlingLogin, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_LOGOUTSERVER,
      std::bind(&SyncLogic::handlingLogout, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));

  m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
      ServiceType::SERVICE_FILEUPLOADREQUEST,
      std::bind(&SyncLogic::handlingFileUploading, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3)));
}

void SyncLogic::commit(pair recv_node) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  if (m_queue.size() > ServerConfig::get_instance()->ResourceQueueSize) {
    spdlog::warn("SyncLogic Queue is full!");
    return;
  }
  m_queue.push(std::move(recv_node));
  m_cv.notify_one();
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
      spdlog::error("Service Type Not Found!");
      return;
    }
    m_callbacks[type](type, session, std::move(node.second));
  } catch (const std::exception &e) {
    spdlog::error("Excute Method Failed, Internel Server Error! Error Code {}",
                  e.what());
  }
}

void SyncLogic::shutdown() {
  m_stop = true;
  m_cv.notify_all();

  /*join the working thread*/
  if (m_working.joinable()) {
    m_working.join();
  }
}

void SyncLogic::handlingLogin(ServiceType srv_type,
                              std::shared_ptr<Session> session, NodePtr recv) {

  boost::json::object src_obj;
  boost::json::object result;

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

  spdlog::info("[UUID = {}] Trying to login to ResourcesServer with Token {}",
               uuid, token);

  auto uuid_value_op = tools::string_to_value<std::size_t>(uuid);
  if (!uuid_value_op.has_value()) {
    generateErrorMessage("Failed to convert string to number",
                         ServiceType::SERVICE_LOGINRESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);
    return;
  }

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

  /*send it back*/
  session->sendMessage(ServiceType::SERVICE_LOGINRESPONSE,
                       boost::json::serialize(result));
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

void SyncLogic::handlingFileUploading(ServiceType srv_type,
                                      std::shared_ptr<Session> session,
                                      NodePtr recv) {

  boost::json::object src_obj;
  boost::json::object dst_root;

  /*output file*/
  std::ofstream out;

  std::optional<std::string> body = recv->get_msg_body();
  /*recv message error*/
  if (!body.has_value()) {
    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    spdlog::warn("[Resources Server]: Msg Body Parse Error!");
    return;
  }

  // prevent parse error
  try {
    src_obj = boost::json::parse(body.value()).as_object();
  } catch (const boost::json::system_error &e) {

    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    spdlog::warn("[Resources Server]: Json Obj Parse Error! Reason = {}",
                 e.what());
    return;
  }

  // Parsing json object
  if (!(src_obj.contains("filename") && src_obj.contains("checksum") &&
        src_obj.contains("file_size") && src_obj.contains("block") &&
        src_obj.contains("cur_size") && src_obj.contains("cur_seq") &&
        src_obj.contains("last_seq") && src_obj.contains("EOF"))) {

    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);
    spdlog::warn(
        "[Resources Server]: Json Obj Does Not Contains Specific Field!");
    return;
  }

  auto filename = boost::json::value_to<std::string>(src_obj["filename"]);
  auto checksum = boost::json::value_to<std::string>(src_obj["checksum"]);
  auto last_seq = boost::json::value_to<std::string>(src_obj["last_seq"]);
  auto cur_seq = boost::json::value_to<std::string>(src_obj["cur_seq"]);

  auto cur_size_op = tools::string_to_value<std::size_t>(
      boost::json::value_to<std::string>(src_obj["cur_size"]));
  auto total_size_op = tools::string_to_value<std::size_t>(
      boost::json::value_to<std::string>(src_obj["file_size"]));

  if (!cur_size_op.has_value() || !total_size_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::FILE_UPLOAD_ERROR, session);
    return;
  }

  auto cur_size = cur_size_op.value();
  auto total_size = total_size_op.value();

  std::filesystem::path output_dir = ServerConfig::get_instance()->outputPath;
  std::filesystem::path full_path = output_dir / filename;

  std::error_code ec;
  if (!std::filesystem::exists(output_dir)) {
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
      spdlog::error("[Resources Server]: Failed to create directory '{}': {}",
                    output_dir.string(), ec.message());
      generateErrorMessage("Directory Creation Failed",
                           ServiceType::SERVICE_FILEUPLOADRESPONSE,
                           ServiceStatus::FILE_CREATE_ERROR, session);
      return;
    }
  }

  std::filesystem::path target_path =
      std::filesystem::weakly_canonical(full_path, ec);
  if (ec) {
    spdlog::warn(
        "[Resources Server]: Failed to get canonical path for '{}': {}",
        full_path.string(), ec.message());
    generateErrorMessage("Path Canonicalization Error",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::FILE_CREATE_ERROR, session);
    return;
  }

  /*if it is first package then we should create a new file*/
  bool isFirstPackage = (boost::json::value_to<std::string>(
                             src_obj["cur_seq"]) == std::string("1"));

  /*convert base64 to binary*/
  std::string block_data;
  absl::Base64Unescape(boost::json::value_to<std::string>(src_obj["block"]),
                       &block_data);

  out.open(target_path, isFirstPackage
                            ?
                            /*if this is the first package*/ std::ios::binary |
                                std::ios::trunc
                            :
                            /*append mode*/ std::ios::binary | std::ios::app);

  if (!out.is_open()) {
    spdlog::warn("Uploading File [{}] {} Error!", filename,
                 isFirstPackage ? std::string("Created")
                                : std::string("Opened"));

    generateErrorMessage(isFirstPackage ? "File Created Error"
                                        : "File Opened Error",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         isFirstPackage ? ServiceStatus::FILE_CREATE_ERROR
                                        : ServiceStatus::FILE_OPEN_ERROR,
                         session);
    return;
  }

  spdlog::info("[Resources Server]: Uploading {} Progress {:.2f}% ({}/{})",
               filename, static_cast<float>(cur_size) / total_size * 100,
               cur_size, total_size);

  // out.seekp(cur_size);
  out.write(block_data.data(), block_data.size());

  if (!out) {
    spdlog::warn("Uploading File [{}] Write Error!", filename);
    generateErrorMessage("File Write Error",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::FILE_WRITE_ERROR, session);
    return;
  }

  out.close();

  dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
  dst_root["filename"] = filename;
  dst_root["curr_seq"] = cur_seq;
  dst_root["curr_size"] = std::to_string(cur_size);
  dst_root["total_size"] = std::to_string(total_size);
  session->sendMessage(ServiceType::SERVICE_FILEUPLOADRESPONSE,
                       boost::json::serialize(dst_root));
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
