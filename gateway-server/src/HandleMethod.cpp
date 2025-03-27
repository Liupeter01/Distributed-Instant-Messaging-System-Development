#include <grpc/GrpcBalanceService.hpp>
#include <grpc/GrpcVerificationService.hpp>
#include <handler/HandleMethod.hpp>
#include <http/HttpConnection.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json.hpp>
#include <redis/RedisManager.hpp>
#include <spdlog/spdlog.h>
#include <sql/MySQLConnectionPool.hpp>

HandleMethod::~HandleMethod() {}

HandleMethod::HandleMethod() {
  /*register both get and post callbacks*/
  registerCallBacks();
}

void HandleMethod::registerGetCallBacks() {}

void HandleMethod::registerPostCallBacks() {
  this->post_method_callback.emplace(
      "/get_verification",
      [this](std::shared_ptr<HTTPConnection> conn) -> bool {
        conn->http_response.set(boost::beast::http::field::content_type,
                                "text/json");
        auto body =
            boost::beast::buffers_to_string(conn->http_request.body().data());

        spdlog::info("Server receive post data: {}", body.c_str());

        boost::json::object send_obj; /*write into body*/
        boost::json::object src_obj;    /*store json from client*/

        // prevent parse error
        try {
                  src_obj = boost::json::parse(body).as_object();
        }
        catch (const boost::json::system_error& e) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        // Parsing failed
        if (!src_obj.contains("email")) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        /*Get email string and send to grpc service*/
        auto email = boost::json::value_to<std::string>(src_obj["email"]);

        spdlog::info("Server receive verification request, email addr: {}",
                     email.c_str());

        auto response = gRPCVerificationService::getVerificationCode(email);

        send_obj["error"] = response.error();
        send_obj["email"] = email;
        boost::beast::ostream(conn->http_response.body())
                  << boost::json::serialize(send_obj);
        return true;
      });

  this->post_method_callback.emplace(
      "/post_registration",
      [this](std::shared_ptr<HTTPConnection> conn) -> bool {
        conn->http_response.set(boost::beast::http::field::content_type,
                                "text/json");
        auto body =
            boost::beast::buffers_to_string(conn->http_request.body().data());

        spdlog::info("Server receive registration request, post data: {}",
                     body.c_str());

        boost::json::object send_obj; /*write into body*/
        boost::json::object src_obj;    /*store json from client*/

        // prevent parse error
        try {
                  src_obj = boost::json::parse(body).as_object();
        }
        catch (const boost::json::system_error& e) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        // Parsing failed
        if (!(src_obj.contains("username") && src_obj.contains("password") &&
                  src_obj.contains("email") && src_obj.contains("cpatcha"))) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        /*Get email string and send to grpc service*/
        auto username = boost::json::value_to<std::string>(src_obj["username"]);
        auto  password = boost::json::value_to<std::string>(src_obj["password"]);
        auto email = boost::json::value_to<std::string>(src_obj["email"]);
        auto  cpatcha = boost::json::value_to<std::string>(src_obj["cpatcha"]);

        /*find verification code by checking email in redis*/
        connection::ConnectionRAII<redis::RedisConnectionPool,
                                   redis::RedisContext>
            raii;

        std::optional<std::string> verification_code =
            raii->get()->checkValue(email);

        /*
         * Redis
         * no verification code found!!
         */
        if (!verification_code.has_value()) {
          generateErrorMessage("Internel redis server error!",
                               ServiceStatus::REDIS_UNKOWN_ERROR, conn);
          return false;
        }

        if (verification_code.value() != cpatcha) {
          generateErrorMessage("CPATCHA is different from Redis DB!",
                               ServiceStatus::REDIS_CPATCHA_NOT_FOUND, conn);
          return false;
        }

        MySQLRequestStruct request;
        request.m_username = username;
        request.m_password = password;
        request.m_email = email;

        /*MYSQL(start to create a new user)*/
        connection::ConnectionRAII<mysql::MySQLConnectionPool,
                                   mysql::MySQLConnection>
            mysql;

        if (!mysql->get()->registerNewUser(std::move(request))) {
          generateErrorMessage("MYSQL user register error",
                               ServiceStatus::MYSQL_INTERNAL_ERROR, conn);
          return false;
        }

        /*get uuid by username*/
        std::optional<std::size_t> res =
            mysql->get()->getUUIDByUsername(username);
        if (!res.has_value()) {
          generateErrorMessage("No UUID related to Username",
                               ServiceStatus::LOGIN_UNSUCCESSFUL, conn);
          return false;
        }

        send_obj["error"] =
            static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
        send_obj["username"] = username;
        send_obj["password"] = password;
        send_obj["email"] = email;

        /*get required uuid, and return it back to user!*/
        send_obj["uuid"] = std::to_string(res.value());

        boost::beast::ostream(conn->http_response.body())
            << boost::json::serialize(send_obj);
        return true;
      });

  this->post_method_callback.emplace(
      "/check_accountexists",
      [this](std::shared_ptr<HTTPConnection> conn) -> bool {
        conn->http_response.set(boost::beast::http::field::content_type,
                                "text/json");
        auto body =
            boost::beast::buffers_to_string(conn->http_request.body().data());

        spdlog::info("Server receive registration request, post data: {}",
                     body.c_str());

        boost::json::object send_obj; /*write into body*/
        boost::json::object src_obj;    /*store json from client*/

        // prevent parse error
        try {
                  src_obj = boost::json::parse(body).as_object();
        }
        catch (const boost::json::system_error& e) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        // Parsing failed
        if (!(src_obj.contains("username") &&   src_obj.contains("email") )) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        /*Get email string and send to grpc service*/
        auto username = boost::json::value_to<std::string>(src_obj["username"]);
        auto email = boost::json::value_to<std::string>(src_obj["email"]);

        /*MYSQL(check exist)*/
        connection::ConnectionRAII<mysql::MySQLConnectionPool,
                                   mysql::MySQLConnection>
            mysql;

        if (!mysql->get()->checkAccountAvailability(username, email)) {
          generateErrorMessage("MYSQL account not exists",
                               ServiceStatus::MYSQL_ACCOUNT_NOT_EXISTS, conn);
          return false;
        }

        send_obj["error"] =
            static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
        send_obj["username"] = username;
        send_obj["email"] = email;

        boost::beast::ostream(conn->http_response.body())
                  << boost::json::serialize(send_obj);
        return true;
      });

  this->post_method_callback.emplace(
      "/reset_password", [this](std::shared_ptr<HTTPConnection> conn) -> bool {
        conn->http_response.set(boost::beast::http::field::content_type,
                                "text/json");
        auto body =
            boost::beast::buffers_to_string(conn->http_request.body().data());

        spdlog::info("Server receive registration request, post data: {}",
                     body.c_str());

        boost::json::object send_obj; /*write into body*/
        boost::json::object src_obj;    /*store json from client*/

        // prevent parse error
        try {
                  src_obj = boost::json::parse(body).as_object();
        }
        catch (const boost::json::system_error& e) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        // Parsing failed
        if (!(src_obj.contains("username") && src_obj.contains("password") &&
                  src_obj.contains("email"))) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        /*Get email string and send to grpc service*/
        auto username = boost::json::value_to<std::string>(src_obj["username"]);
        auto  password = boost::json::value_to<std::string>(src_obj["password"]);
        auto email = boost::json::value_to<std::string>(src_obj["email"]);

        MySQLRequestStruct request;
        request.m_username = username;
        request.m_password = password;
        request.m_email = email;

        /*MYSQL(update table)*/
        connection::ConnectionRAII<mysql::MySQLConnectionPool,
                                   mysql::MySQLConnection>
            mysql;

        if (!mysql->get()->alterUserPassword(std::move(request))) {
          generateErrorMessage("Missing critical info",
                               ServiceStatus::MYSQL_MISSING_INFO, conn);
          return false;
        }

        send_obj["error"] =
            static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);

        boost::beast::ostream(conn->http_response.body())
            << boost::json::serialize(send_obj);
        return true;
      });

  this->post_method_callback.emplace(
      "/trylogin_server", [this](std::shared_ptr<HTTPConnection> conn) -> bool {
        conn->http_response.set(boost::beast::http::field::content_type,
                                "text/json");
        auto body =
            boost::beast::buffers_to_string(conn->http_request.body().data());

        spdlog::info("Server receive server allocation request, post data: {}",
                     body.c_str());

        boost::json::object send_obj; /*write into body*/
        boost::json::object src_obj;    /*store json from client*/

        // prevent parse error
        try {
                  src_obj = boost::json::parse(body).as_object();
        }
        catch (const boost::json::system_error& e) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        // Parsing failed
        if (!(src_obj.contains("username") && src_obj.contains("password"))) {
                  generateErrorMessage("Failed to parse json data",
                            ServiceStatus::JSONPARSE_ERROR, conn);
                  return false;
        }

        /*Get email string and send to grpc service*/
        auto username = boost::json::value_to<std::string>(src_obj["username"]);
        auto password = boost::json::value_to<std::string>(src_obj["password"]);

        /*MYSQL(select username & password and retrieve uuid)*/
        connection::ConnectionRAII<mysql::MySQLConnectionPool,
                                   mysql::MySQLConnection>
            mysql;

        /*check account credential*/
        [[maybe_unused]] std::optional<std::size_t> res =
            mysql->get()->checkAccountLogin(username, password);
        if (!res.has_value()) {
          generateErrorMessage("Wrong username or password",
                               ServiceStatus::LOGIN_INFO_ERROR, conn);
          return false;
        }

        /*get uuid by username*/
        res = mysql->get()->getUUIDByUsername(username);
        if (!res.has_value()) {
          generateErrorMessage("No UUID related to Username",
                               ServiceStatus::LOGIN_UNSUCCESSFUL, conn);
          return false;
        }

        std::size_t uuid = res.value();

        /*
         *pass user's uuid parameter to the server, and returns available server
         *address to user
         */
        auto response = gRPCBalancerService::addNewUserToServer(uuid);

        if (response.error() !=
            static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS)) {
          spdlog::error("[client {}] try login server failed!, error code {}",
                        std::to_string(uuid), response.error());
        }

        send_obj["uuid"] = std::to_string(uuid);
        send_obj["error"] = response.error();
        send_obj["host"] = response.host();
        send_obj["port"] = response.port();
        send_obj["token"] = response.token();

        boost::beast::ostream(conn->http_response.body())
                  << boost::json::serialize(send_obj);
        return true;
      });
}

void HandleMethod::generateErrorMessage(std::string_view message,
                                        ServiceStatus status,
                                        std::shared_ptr<HTTPConnection> conn) {

  boost::json::object obj;
  obj["error"] = static_cast<uint8_t>(status);
  spdlog::error(message);
  boost::beast::ostream(conn->http_response.body()) << boost::json::serialize(obj);
}

void HandleMethod::registerCallBacks() {
  registerGetCallBacks();
  registerPostCallBacks();
}

bool HandleMethod::handleGetMethod(
    std::string str, std::shared_ptr<HTTPConnection> extended_lifetime) {
  /*Callback Func Not Found*/
  if (get_method_callback.find(str) == get_method_callback.end()) {
    return false;
  }
  get_method_callback[str](extended_lifetime);
  return true;
}

bool HandleMethod::handlePostMethod(
    std::string str, std::shared_ptr<HTTPConnection> extended_lifetime) {
  /*Callback Func Not Found*/
  if (post_method_callback.find(str) == post_method_callback.end()) {
    return false;
  }
  [[maybe_unused]] bool res = post_method_callback[str](extended_lifetime);
  return true;
}
