#include <absl/strings/escaping.h> /*base64*/
#include <config/ServerConfig.hpp>
#include <dispatcher/FileProcessingDispatcher.hpp>
#include <sql/MySQLConnection.hpp>
#include <filesystem>
#include <fstream>
#include <handler/RequestHandlerNode.hpp>
#include <server/AsyncServer.hpp>
#include <server/FileHasherLogger.hpp>
#include <server/Session.hpp>
#include <server/UserNameCard.hpp>
#include <spdlog/spdlog.h>
#include <tools/tools.hpp>

namespace handler {

          /*redis*/
          std::string RequestHandlerNode::redis_server_login = "redis_server";

          /*store user base info in redis*/
          std::string RequestHandlerNode::user_prefix = "user_info_";

          /*store the server name that this user belongs to*/
          std::string RequestHandlerNode::server_prefix = "uuid_";

          RequestHandlerNode::RequestHandlerNode() : RequestHandlerNode(0) {}

          RequestHandlerNode::RequestHandlerNode(const std::size_t id)
                    : m_stop(false), handler_id(id) {

                    /*register callbacks*/
                    registerCallbacks();

                    /*start processing thread to process queue*/
                    m_working = std::thread(&RequestHandlerNode::processing, this);
          }

          RequestHandlerNode::~RequestHandlerNode() { shutdown(); }

          void RequestHandlerNode::registerCallbacks() {

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_LOGINSERVER,
                              std::bind(&RequestHandlerNode::handlingLogin, this, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3)));

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_LOGOUTSERVER,
                              std::bind(&RequestHandlerNode::handlingLogout, this,
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3)));

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_FILEUPLOADREQUEST,
                              std::bind(&RequestHandlerNode::handlingFileUploading, this,
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3)));

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_AVATARUPLOADREQUEST,
                              std::bind(&RequestHandlerNode::handlingAvatarUploading, this,
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3)));

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_FILECHECKUPLOADPROGRESSREQUEST,
                              std::bind(&RequestHandlerNode::handlingCheckUploadProgress, this,
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3)));

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_INITFILEFETCHINGREQUEST,
                              std::bind(&RequestHandlerNode::handlingFileDownloading, this,
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3)));

                    m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                              ServiceType::SERVICE_FILEDOWNLOADREQUEST,
                              std::bind(&RequestHandlerNode::handlingFileDownloading, this,
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3)));
          }

          void RequestHandlerNode::commit(
                    pair recv_node, [[maybe_unused]] SessionPtr live_extend) {

                    std::lock_guard<std::mutex> _lckg(m_mtx);
                    // if (m_queue.size() > ServerConfig::get_instance()->ResourceQueueSize) {
                    //   spdlog::warn("[{}]: RequestHandlerNode {}'s Queue is full!",
                    //             ServerConfig::get_instance()->GrpcServerName,
                    //                handler_id);
                    //   return;
                    // }

                    m_queue.push(std::move(recv_node));
                    m_cv.notify_one();
          }

          /*parse Json*/
          bool RequestHandlerNode::parseJson(std::shared_ptr<Session> session,
                    NodePtr& recv,
                    boost::json::object& src_obj) {
                    std::optional<std::string> body = recv->get_msg_body();

                    if (!body) {
                              generateErrorMessage("Failed to parse JSON data",
                                        ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                                        ServiceStatus::JSONPARSE_ERROR, session);
                              return false;
                    }

                    try {
                              src_obj = boost::json::parse(body.value()).as_object();
                    }
                    catch (const boost::json::system_error& e) {
                              generateErrorMessage("Invalid JSON format",
                                        ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                                        ServiceStatus::JSONPARSE_ERROR, session);
                              return false;
                    }
                    return true;
          }

          void RequestHandlerNode::generateErrorMessage(const std::string& log,
                    ServiceType type,
                    ServiceStatus status,
                    SessionPtr conn) {

                    boost::json::object obj;
                    obj["error"] = static_cast<uint8_t>(status);
                    spdlog::warn("[{}]: {}", ServerConfig::get_instance()->GrpcServerName, log);
                    conn->sendMessage(type, boost::json::serialize(obj), conn);
          }

          void RequestHandlerNode::processing() {
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

                              auto& front = m_queue.front();
                              execute(std::move(m_queue.front()));
                              m_queue.pop();
                    }
          }

          void RequestHandlerNode::execute(pair&& node) {
                    std::shared_ptr<Session> session = node.first;

                    ServiceType type = static_cast<ServiceType>(node.second->_id);

                    try {
                              /*executing callback on specific type*/
                              auto it = m_callbacks.find(type);
                              if (it == m_callbacks.end()) {
                                        spdlog::error("[{}]: RequestHandlerNode::execute ervice Type Not Found!",
                                                  ServerConfig::get_instance()->GrpcServerName);
                                        return;
                              }
                              m_callbacks[type](type, session, std::move(node.second));
                    }
                    catch (const std::exception& e) {
                              spdlog::error("[{}]: RequestHandlerNode::execute Method Failed, Internel "
                                        "Server Error! Error Code {}",
                                        ServerConfig::get_instance()->GrpcServerName, e.what());
                    }
          }

          void RequestHandlerNode::shutdown() {
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
          void RequestHandlerNode::incrementConnection() {
                    RedisRAII raii;

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
          void RequestHandlerNode::decrementConnection() {
                    RedisRAII raii;

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

          bool RequestHandlerNode::tagCurrentUser(const std::string& uuid) {
                    RedisRAII raii;
                    return raii->get()->setValue(server_prefix + uuid,
                              ServerConfig::get_instance()->GrpcServerName);
          }

          bool RequestHandlerNode::untagCurrentUser(const std::string& uuid) {
                    RedisRAII raii;
                    return raii->get()->delPair(server_prefix + uuid);
          }

          void RequestHandlerNode::handlingLogin(
                    ServiceType srv_type, std::shared_ptr<Session> session, NodePtr recv) {

                    boost::json::object src_obj;
                    boost::json::object result;

                    parseJson(session, recv, src_obj);

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
                              boost::json::serialize(result), session);
          }

          void RequestHandlerNode::handlingLogout(
                    ServiceType srv_type, std::shared_ptr<Session> session, NodePtr recv) {

                    /*
                     * sub user connection counter for current server
                     * 1. HGET not exist: Current Chatting server didn't setting up connection
                     * counter
                     * 2. HGET exist: Decrement by 1
                     */
                    decrementConnection();

                    /*delete user belonged server in redis*/
                    if (!untagCurrentUser(session->get_user_uuid())) {
                              spdlog::warn("[UUID = {}] Unbind Current User From Current Server {}",
                                        session->get_user_uuid(),
                                        ServerConfig::get_instance()->GrpcServerName);
                    }
          }

          void RequestHandlerNode::handlingAvatarUploading(ServiceType srv_type,
                    std::shared_ptr<Session> session, NodePtr recv) {

                    std::string block_data;
                    auto reqOpt = parseUploadRequest(session, recv, block_data);
                    if (!reqOpt.has_value()) {
                              generateErrorMessage("Failed to parse json data",
                                        srv_type,
                                        ServiceStatus::JSONPARSE_ERROR,
                                        session);
                              return;
                    }

                    auto req = reqOpt.value();

                    submitUploadTask(
                              req,
                              std::move(block_data),
                              session,
                              ServiceType::SERVICE_AVATARUPLOADRESPONSE,
                              [session](ServiceStatus status, std::size_t curr_size, std::shared_ptr<FileHasherDesc> desc) {

                                        if (status == ServiceStatus::SERVICE_SUCCESS) {

                                                  //File Transfer ended?
                                                  if (desc->isEOF == "1") {

                                                            MySQLRAII raii;

                                                            if (!raii->get()->updateUserProfileAvatar(std::stoi(desc->uuid), desc->key)) {
                                                                      spdlog::error("[{}]: Update UUID = {} Userprofile Avatar Failed!",
                                                                                ServerConfig::get_instance()->GrpcServerName, desc->uuid);

                                                                      return ServiceStatus::DATABASE_ERROR;
                                                            }

                                                            spdlog::info("[{}]: Avatar updated successfully for uuid={}",
                                                                      ServerConfig::get_instance()->GrpcServerName,
                                                                      desc->uuid);
                                                  }
                                                  return status;

                                        }});
          }

          void RequestHandlerNode::handlingFileUploading(
                    ServiceType srv_type, std::shared_ptr<Session> session, NodePtr recv) {

                    std::string block_data;
                    auto reqOpt = parseUploadRequest(session, recv, block_data);
                    if (!reqOpt.has_value()) {
                              generateErrorMessage("Failed to parse json data",
                                        srv_type,
                                        ServiceStatus::JSONPARSE_ERROR,
                                        session);
                              return;
                    }

                    auto req = reqOpt.value();

                    submitUploadTask(
                              req,
                              std::move(block_data),
                              session,
                              ServiceType::SERVICE_FILEUPLOADRESPONSE,
                              [session](ServiceStatus status, [[maybe_unused]] std::size_t , [[maybe_unused]] std::shared_ptr<FileHasherDesc> ) {
                                        
                                        return status;
                              });
          }

          void RequestHandlerNode::handlingFileDownloading(
                    ServiceType srv_type, std::shared_ptr<Session> session, NodePtr recv) {

                    boost::json::object src_obj;
                    boost::json::object dst_root;

                    parseJson(session, recv, src_obj);

                    // Parsing json object
                    if (!(src_obj.contains("uuid") && src_obj.contains("filename") &&
                              src_obj.contains("filepath") &&
                              src_obj.contains("current_block_size") &&
                              src_obj.contains("transfered_size") && src_obj.contains("cur_seq") &&
                              src_obj.contains("EOF"))) {

                              generateErrorMessage("Failed to parse json data",
                                        ServiceType::SERVICE_FILEDOWNLOADRESPONSE,
                                        ServiceStatus::JSONPARSE_ERROR, session);
                              return;
                    }

                    [[maybe_unused]] auto uuid =
                              boost::json::value_to<std::string>(src_obj["uuid"]);
                    [[maybe_unused]] auto filename =
                              boost::json::value_to<std::string>(src_obj["filename"]);
                    [[maybe_unused]] auto cur_seq =
                              boost::json::value_to<std::string>(src_obj["cur_seq"]);
                    [[maybe_unused]] auto filepath =
                              boost::json::value_to<std::string>(src_obj["filepath"]);
                    [[maybe_unused]] auto eof =
                              boost::json::value_to<std::string>(src_obj["EOF"]);
                    [[maybe_unused]] auto current_block_size = std::stoi(
                              boost::json::value_to<std::string>(src_obj["current_block_size"]));
                    [[maybe_unused]] auto transfered_size =
                              std::stoi(boost::json::value_to<std::string>(src_obj["transfered_size"]));

                    std::filesystem::path output_dir = ServerConfig::get_instance()->outputPath;
                    std::filesystem::path full_path = output_dir / uuid / filename;

                    /*
                     * if it is first package then we should create a new file
                     * if it is end of the file
                     */
                    bool isEOF = (eof == std::string("1"));

                    /*
                     * We do not need to know wheather the FileHasherDesc exist OR NOT
                     * Just Allocate a block with newest data and REPLACE the old one / INSERT the
                     * new one It's READ ONLY!
                     */
                    auto desc = std::make_shared<FileHasherDesc>(
                              uuid, filename, std::string{}, filepath, cur_seq, std::string{}, eof,
                              transfered_size, current_block_size,
                              std::numeric_limits<std::size_t>::max(), TransferDirection::Download);

                    if (srv_type != ServiceType::SERVICE_INITFILEFETCHINGREQUEST) {

                              if (!(src_obj.contains("checksum") && src_obj.contains("total_size") &&
                                        src_obj.contains("last_seq"))) {

                                        generateErrorMessage("Failed to parse json data",
                                                  ServiceType::SERVICE_FILEDOWNLOADRESPONSE,
                                                  ServiceStatus::JSONPARSE_ERROR, session);
                                        return;
                              }

                              [[maybe_unused]] auto checksum =
                                        boost::json::value_to<std::string>(src_obj["checksum"]);
                              [[maybe_unused]] auto total_size =
                                        std::stoi(boost::json::value_to<std::string>(src_obj["total_size"]));
                              [[maybe_unused]] auto last_seq =
                                        boost::json::value_to<std::string>(src_obj["last_seq"]);

                              desc->checksum = checksum;
                              desc->total_size = total_size;
                              desc->last_sequence = last_seq;
                    }

                    auto callback =
                              [/*life length*/ session, uuid, cur_seq, filename, filepath,
                              transfered_size, current_block_size,
                              isEOF](const ServiceStatus status,
                                        std::unique_ptr<FileDownloadDescription::DownloadInfo> info =
                                        nullptr) {
                                                  boost::json::object obj;

                                                  if (status != ServiceStatus::SERVICE_SUCCESS && !info) {
                                                            obj["error"] = static_cast<uint8_t>(status);
                                                  }
                                                  else {
                                                            obj["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
                                                            obj["uuid"] = uuid;
                                                            obj["filename"] = filename;
                                                            obj["filepath"] = filepath;
                                                            obj["checksum"] = info->checksum;
                                                            obj["curr_seq"] = cur_seq;
                                                            obj["last_seq"] = info->last_seq;

                                                            // transfered: the accumlated transfered size in the perviou rounds
                                                            // current_block_size: the transfered size in the last round
                                                            obj["curr_size"] = std::to_string(transfered_size);
                                                            obj["total_size"] = info->total_size;

                                                            /*End Of File*/
                                                            obj["EOF"] = isEOF ? true : false;
                                                  }
                                                  session->sendMessage(ServiceType::SERVICE_FILEUPLOADRESPONSE,
                                                            boost::json::serialize(obj), session);
                              };

                    auto file_chunk = std::make_unique<FileDownloadDescription>(
                              *desc, std::move(callback));

                    dispatcher::FileProcessingDispatcher::get_instance()->commit(
                              std::move(file_chunk), session);
          }

          void RequestHandlerNode::handlingCheckUploadProgress(
                    ServiceType srv_type, std::shared_ptr<Session> session, NodePtr recv) {

                    boost::json::object src_obj;
                    boost::json::object dst_root;

                    parseJson(session, recv, src_obj);

                    // Parsing failed
                    if (!(src_obj.contains("filename") && src_obj.contains("checksum"))) {

                              generateErrorMessage("Failed to parse json data",
                                        ServiceType::SERVICE_FILECHECKUPLOADPROGRESSRESPONSE,
                                        ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                              return;
                    }

                    std::string filename =
                              boost::json::value_to<std::string>(src_obj["filename"]);
                    std::string checksum =
                              boost::json::value_to<std::string>(src_obj["checksum"]);

                    /*now the records are in redis!*/
                    std::string key = filename + "_" + checksum;
                    auto status = FileHasherLogger::get_instance()->getFileDescBlock(key);

                    if (!status.has_value()) {
                              // Not Block Found
                              dst_root["error"] = static_cast<uint8_t>(ServiceStatus::FILE_NOT_FOUND);
                    }
                    else {
                              auto opt = status.value();
                              dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
                              dst_root["uuid"] = opt->uuid;
                              dst_root["filename"] = opt->filename;
                              dst_root["filepath"] = opt->filePath;
                              dst_root["checksum"] = opt->checksum;

                              //
                              dst_root["curr_seq"] = opt->curr_sequence;
                              dst_root["last_seq"] = opt->last_sequence;

                              // transfered: the accumlated transfered size in the perviou rounds
                              // current_block_size: the transfered size in the last round
                              dst_root["curr_size"] = opt->transfered_size;
                              dst_root["total_size"] = opt->total_size;

                              /*End Of File*/
                              dst_root["EOF"] = opt->isEOF;
                    }

                    session->sendMessage(ServiceType::SERVICE_FILECHECKUPLOADPROGRESSRESPONSE,
                              boost::json::serialize(dst_root), session);
          }

          std::optional<std::shared_ptr<FileHasherDesc>>
           RequestHandlerNode::parseUploadRequest(std::shared_ptr<Session> session, NodePtr& recv, std::string& block_data) {

                    boost::json::object src_obj;
                    parseJson(session, recv, src_obj);

                    // Parsing json object
                    if (!(src_obj.contains("uuid") && 
                              src_obj.contains("filename") &&
                              src_obj.contains("checksum") && 
                              src_obj.contains("filepath") &&
                              src_obj.contains("total_size") &&
                              src_obj.contains("current_block_size") &&
                              src_obj.contains("transfered_size") &&
                              src_obj.contains("cur_seq") &&
                              src_obj.contains("block") && 
                              src_obj.contains("last_seq") &&
                              src_obj.contains("EOF"))) {

                              return std::nullopt;
                    }

                    try {
                              [[maybe_unused]] auto uuid =
                                        boost::json::value_to<std::string>(src_obj["uuid"]);
                              [[maybe_unused]] auto filename =
                                        boost::json::value_to<std::string>(src_obj["filename"]);
                              [[maybe_unused]] auto checksum =
                                        boost::json::value_to<std::string>(src_obj["checksum"]);
                              [[maybe_unused]] auto filepath =
                                        boost::json::value_to<std::string>(src_obj["filepath"]);
                              [[maybe_unused]] auto last_seq =
                                        boost::json::value_to<std::string>(src_obj["last_seq"]);
                              [[maybe_unused]] auto cur_seq =
                                        boost::json::value_to<std::string>(src_obj["cur_seq"]);
                              [[maybe_unused]] auto transfered_size =
                                        std::stoi(boost::json::value_to<std::string>(src_obj["transfered_size"]));
                              [[maybe_unused]] auto current_block_size = std::stoi(
                                        boost::json::value_to<std::string>(src_obj["current_block_size"]));
                              [[maybe_unused]] auto total_size =
                                        std::stoi(boost::json::value_to<std::string>(src_obj["total_size"]));
                              [[maybe_unused]] auto eof =
                                        boost::json::value_to<std::string>(src_obj["EOF"]);

                              block_data = std::move(boost::json::value_to<std::string>(src_obj["block"]));

                             return std::make_shared<FileHasherDesc>(
                                        uuid, filename, checksum, filepath, cur_seq, last_seq, eof,
                                        transfered_size, current_block_size, total_size,
                                        TransferDirection::Upload);
                    }
                    catch (const std::exception& e) {
                              spdlog::error("[{}]: Failed to parse upload request: {}",
                                        ServerConfig::get_instance()->GrpcServerName,
                                        e.what());
                              return std::nullopt;
                    }
          }

          void   RequestHandlerNode::submitUploadTask(
                    std::shared_ptr<FileHasherDesc> desc,
                    std::string&& block_data,
                    std::shared_ptr<Session> session,
                    ServiceType response_type,
                    std::function<ServiceStatus(ServiceStatus, std::size_t, std::shared_ptr<FileHasherDesc>)> businessHandler) {

                    auto callback = [session, desc, response_type, businessHandler](
                              const ServiceStatus status,
                              const std::size_t curr_size) {

                                        ServiceStatus finalStatus = status;

                                        if (status == ServiceStatus::SERVICE_SUCCESS) {
                                                  finalStatus = businessHandler(status, curr_size, desc);
                                        }

                                        sendUploadResponse(session, response_type, desc, status, curr_size);
                              };

                    auto file_chunk = std::make_unique<handler::FileUploadDescription>(
                              *desc,
                              std::move(block_data), 
                              std::move(callback));

                    dispatcher::FileProcessingDispatcher::get_instance()->commit(
                              std::move(file_chunk), session);
          }

         void RequestHandlerNode::sendUploadResponse(
                    std::shared_ptr<Session> session,
                    ServiceType type,
                    std::shared_ptr<FileHasherDesc> req,
                    ServiceStatus status,
                    std::size_t curr_size) {

                    boost::json::object obj;

                    obj["error"] = static_cast<uint8_t>(status);

                    if (status == ServiceStatus::SERVICE_SUCCESS) {
                              obj["uuid"] = req->uuid;
                              obj["filename"] = req->filename;
                              obj["filepath"] = req->filePath;
                              obj["checksum"] = req->checksum;
                              obj["curr_seq"] = req->curr_sequence;
                              obj["last_seq"] = req->last_sequence;
                              obj["curr_size"] = std::to_string(curr_size);
                              obj["total_size"] = std::to_string(req->total_size);
                              obj["EOF"] = req->isEOF;
                    }

                    session->sendMessage(type, boost::json::serialize(obj), session);
          }
}