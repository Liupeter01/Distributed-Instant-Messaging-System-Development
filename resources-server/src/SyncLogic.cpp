#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <tools/tools.hpp>
#include <absl/strings/escaping.h>      /*base64*/
#include <config/ServerConfig.hpp>
#include <handler/SyncLogic.hpp>
#include <server/AsyncServer.hpp>

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
    return;
  }

  // prevent parse error
  try {
    src_obj = boost::json::parse(body.value()).as_object();

  } catch (const boost::json::system_error &e) {

    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::JSONPARSE_ERROR, session);

    return;
  }

  // Parsing failed
  if (!(src_obj.contains("filename") && src_obj.contains("checksum") &&
        src_obj.contains("block") && src_obj.contains("cur_sql"))) {

    generateErrorMessage("Failed to parse json data",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::LOGIN_UNSUCCESSFUL, session);

    return;
  }

  auto filename = boost::json::value_to<std::string>(src_obj["filename"]);
  auto checksum = boost::json::value_to<std::string>(src_obj["checksum"]);
  auto last_seq = boost::json::value_to<std::string>(src_obj["last_seq"]);
  auto cur_seq = boost::json::value_to<std::string>(src_obj["cur_seq"]);

  auto cur_size_op = 
      tools::string_to_value<std::size_t>(boost::json::value_to<std::string>(src_obj["cur_size"]));
  auto total_size_op =
      tools::string_to_value<std::size_t>(boost::json::value_to<std::string>(src_obj["file_size"]));

  if (!cur_size_op.has_value() || !total_size_op.has_value()) {
    spdlog::warn("Casting string typed key to std::size_t!");
    generateErrorMessage("Internel Server Error",
                         ServiceType::SERVICE_FILEUPLOADRESPONSE,
                         ServiceStatus::FILE_UPLOAD_ERROR, session);
    return;
  }

  auto cur_size = cur_size_op.value();
  auto total_size = total_size_op.value();

  /*if it is first package then we should create a new file*/
  bool isFirstPackage = (boost::json::value_to<std::string>(src_obj["cur_seq"])== std::string("1"));

  /*End of Transmission*/
  if (src_obj.contains("EOF")) {
    // src_root["EOF"].asInt();
  }

  /*convert base64 to binary*/
  std::string block_data;
  absl::Base64Unescape(src_obj["block"].as_string(), &block_data);

  if (isFirstPackage) {
    /*if this is the first package*/
    out.open(filename, std::ios::binary | std::ios::trunc);
  } else {
    /*append mode*/
    out.open(filename, std::ios::binary | std::ios::app);
  }

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
