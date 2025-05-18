#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <config/ServerConfig.hpp>
#include <handler/SyncLogic.hpp>
#include <server/AsyncServer.hpp>
#include <server/Session.hpp>
#include <server/UserManager.hpp>
#include <spdlog/spdlog.h>
#include <tools/tools.hpp>

/*store the current session id that this user belongs to*/
std::string Session::session_prefix = "session_";

/*store the server name that this user belongs to*/
std::string Session::server_prefix = "uuid_";

/*redis*/
std::string Session::redis_server_login = "redis_server";

Session::Session(boost::asio::io_context &_ioc, AsyncServer *my_gate)
    : s_closed(false), s_socket(_ioc), s_gate(my_gate),
      m_write_in_progress(false), m_state(SessionState::Alive),
      m_recv_buffer(std::make_unique<Recv>(
          ByteOrderConverter{})) /*init header buffer init*/
{
  /*generate the session id*/
  this->s_session_id = tools::userTokenGenerator();
}

Session::~Session() {
  if (!s_closed) {
    s_closed = true;
  }
}

void Session::startSession() {
  try {
    boost::asio::async_read(
        s_socket,
        boost::asio::buffer(m_recv_buffer->get_header_base(),
                            m_recv_buffer->get_header_length()),
        std::bind(&Session::handle_header, this, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
  } catch (const std::exception &e) {
    spdlog::error("[{}] startSession {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }
}

void Session::closeSession() {
  if (s_closed)
    return;

  if (s_socket.is_open()) {
    boost::system::error_code ec;
    s_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    s_socket.close(ec);
  }
}

void Session::terminateAndRemoveFromServer(const std::string &user_uuid) {
  UserManager::get_instance()->removeUsrSession(user_uuid);
}

void Session::terminateAndRemoveFromServer(
    const std::string &user_uuid, const std::string &expected_session_id) {
  s_gate->terminateConnection(user_uuid, expected_session_id);
}

void Session::removeRedisCache(const std::string &uuid,
                               const std::string &session_id) {
  /*we need to add distributed-lock here to remove session-id and user id*/
  RedisRAII raii;
  if (auto opt =
          raii->get()->acquire(uuid, 10, 10, redis::TimeUnit::Milliseconds);
      opt) {

    // Get user id which Current UUID Belongs to
    auto redis_user_id = raii->get()->checkValue(server_prefix + uuid);

    // Get Session id which Current UUID Belongs to
    auto redis_session_id = raii->get()->checkValue(session_prefix + uuid);

    if (redis_user_id.has_value() && redis_session_id.has_value()) {
      auto &r_user_id = *redis_user_id;
      auto &r_session_id = *redis_session_id;

      /*
       * If THERE IS NO other server already modify this value
       * THIS USER might already logined on other server, then skip this process
       */
      if (r_session_id == session_id) {
        // Remove the pair relation of server_[uuid]<->[WHICH SERVER]
        raii->get()->delPair(server_prefix + uuid);

        // Remove the pair relation of session_[uuid]<->[SESSION_NUMBER]
        raii->get()->delPair(session_prefix + uuid);
      }
    }

    raii->get()->release(uuid, opt.value());
  }
}

/*
 * IT SHOULD NOT BE DEPLOYED IN TRAVERSAL SCENAIRO
 * UserManager::get_instance()->removeUsrSession(session->get_user_uuid())
 */
void Session::purgeRemoveConnection(std::shared_ptr<Session> session) {

  // remove redis cache, including uuid and session id, Integrated with
  // distributed lock
  removeRedisCache(session->get_user_uuid(), session->get_session_id());

  /*this is the real socket.close method, because the client teminate the
   * connection*/
  UserManager::get_instance()->removeUsrSession(session->get_user_uuid());

  decrementConnection();
}

void Session::sendMessage(ServiceType srv_type, const std::string &message,
                          std::shared_ptr<Session> self) {
  try {
    /*inside SendNode ctor, temporary must be modifiable*/
    std::string temporary = message;

    m_concurrent_sent_queue.push(
        std::make_unique<Send>(static_cast<uint16_t>(srv_type), temporary,
                               ByteOrderConverterReverse{}));

    bool expected = false;
    if (m_write_in_progress.compare_exchange_strong(expected, true)) {
      if (m_concurrent_sent_queue.try_pop(m_current_write_msg)) {

        if (checkDeferredTermination()) {
          m_write_in_progress = false;
          return;
        }

        boost::asio::async_write(
            s_socket,
            boost::asio::buffer(m_current_write_msg->get_header_base(),
                                m_current_write_msg->get_full_length()),
            [self](const boost::system::error_code &ec,
                   std::size_t /*bytes_transferred*/) {
              self->handle_write(self, ec);
            });
      } else {
        m_write_in_progress = false;
      }
    }
  } catch (const std::exception &e) {
    spdlog::error("[{}] Session::sendMessage {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }
}

bool Session::isSessionTimeout(const std::time_t &now) const {
  return std::difftime(now, m_last_heartbeat) >
         static_cast<double>(ServerConfig::get_instance()->heart_beat_timeout);
}

void Session::updateLastHeartBeat() { m_last_heartbeat = std::time(nullptr); }

void Session::handle_write(std::shared_ptr<Session> session,
                           boost::system::error_code ec) {
  try {
    /*error occured*/
    if (ec) {
      spdlog::warn("[{}] Client Session {} UUID {} Exit Anomaly! "
                   "Error message {}",
                   ServerConfig::get_instance()->GrpcServerName,
                   session->s_session_id, session->s_uuid, ec.message());

      purgeRemoveConnection(session);

      return;
    }

    /*till there is no element inside queue*/
    if (m_concurrent_sent_queue.try_pop(m_current_write_msg)) {

      if (checkDeferredTermination()) {
        m_write_in_progress = false;
        return;
      }

      boost::asio::async_write(
          s_socket,
          boost::asio::buffer(m_current_write_msg->get_header_base(),
                              m_current_write_msg->get_full_length()),
          [session](const boost::system::error_code &ec,
                    std::size_t /*bytes_transferred*/) {
            session->handle_write(session, ec);
          });
    } else {
      m_write_in_progress = false;
    }
  } catch (const std::exception &e) {
    spdlog::error("[{}] Session::handle_write {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }
}

void Session::handle_header(std::shared_ptr<Session> session,
                            boost::system::error_code ec,
                            std::size_t bytes_transferred) {
  try {
    /*error occured*/
    if (ec) {
      spdlog::warn("[{}] Client Session {} UUID {} Header Error! Exit Due To "
                   "Header Error "
                   ", Error message {}",
                   ServerConfig::get_instance()->GrpcServerName,
                   session->s_session_id, session->s_uuid, ec.message());

      purgeRemoveConnection(session);
      return;
    }

    /*update remainning data to acquire*/
    m_recv_buffer->update_pointer_pos(bytes_transferred);

    /*current, we didn't get the full size of the header*/
    if (m_recv_buffer->check_header_remaining()) {
      spdlog::warn(
          "[{}] Client Session {} UUID {} Header Error! Exit Due To Transfer "
          "Issue, Only {} Bytes Received!",
          ServerConfig::get_instance()->GrpcServerName, session->s_session_id,
          session->s_uuid, bytes_transferred);

      purgeRemoveConnection(session);
      return;
    }

    /*
     * get msg_id and msg_length
     * and change the network sequence and convert network ----> host
     */
    std::optional<uint16_t> id = m_recv_buffer->get_id();
    if (!id.has_value()) {
      spdlog::warn("[{}] Client Session {} UUID {} Header Error! Invalid ID!",
                   ServerConfig::get_instance()->GrpcServerName,
                   session->s_session_id, session->s_uuid);

      purgeRemoveConnection(session);
      return;
    }

    uint16_t msg_id = id.value();
    if (msg_id >= static_cast<uint16_t>(ServiceType::SERVICE_UNKNOWN)) {
      spdlog::warn(
          "[{}] Client Session {} UUID {} Header Error! Exit Due To Invalid "
          "Service ID {}",
          ServerConfig::get_instance()->GrpcServerName, session->s_session_id,
          session->s_uuid, msg_id);

      purgeRemoveConnection(session);
      return;
    }

    std::optional<uint16_t> length = m_recv_buffer->get_length();
    if (!length.has_value()) {
      spdlog::warn(
          "[{}] Client Session {} UUID {} Header Error! Invalid Length! ",
          ServerConfig::get_instance()->GrpcServerName, session->s_session_id,
          session->s_uuid);

      purgeRemoveConnection(session);
      return;
    }

    uint16_t msg_length = length.value();

    if (msg_length > MAX_LENGTH) {
      spdlog::warn(
          "[{}] Client Session {} UUID {} Header Error! Due To Invalid Data "
          "Length, {} Bytes Received!",
          ServerConfig::get_instance()->GrpcServerName, session->s_session_id,
          session->s_uuid, msg_length);

      purgeRemoveConnection(session);
      return;
    }

    /*update heart beat*/
    updateLastHeartBeat();

    /*for the safty, we have to reset the MsgNode first to prevent memory leak*/
    boost::asio::async_read(
        session->s_socket,
        boost::asio::buffer(m_recv_buffer->get_body_base(), msg_length),
        std::bind(&Session::handle_msgbody, this, session,
                  std::placeholders::_1, std::placeholders::_2));

  } catch (const std::exception &e) {
    spdlog::error("[{}] handle_header {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }
}

void Session::handle_msgbody(std::shared_ptr<Session> session,
                             boost::system::error_code ec,
                             std::size_t bytes_transferred) {
  try {
    /*error occured*/
    if (ec) {

      spdlog::warn(
          "[{}] Client Session {} UUID {} Exit Anomaly! Error message {}",
          ServerConfig::get_instance()->GrpcServerName, session->s_session_id,
          session->s_uuid, ec.message());

      purgeRemoveConnection(session);
      return;
    }

    /*update remainning data to acquire*/
    m_recv_buffer->update_pointer_pos(bytes_transferred);

    /*data is not fully received*/
    if (m_recv_buffer->check_body_remaining()) {
      spdlog::warn("[{}] Client Session {} UUID {} Exit Anomaly! Body Not "
                   "Fully Recevied!",
                   ServerConfig::get_instance()->GrpcServerName,
                   session->s_session_id, session->s_uuid);

      purgeRemoveConnection(session);
      return;
    }

    /*update heart beat*/
    updateLastHeartBeat();

    /*release owner ship of the data, you must release in another unique_ptr*/
    RecvPtr recv(m_recv_buffer.release());

    /*send the received data to SyncLogic to process it */
    SyncLogic::get_instance()->commit(std::make_pair(session, std::move(recv)));

    /*
     * if handle_msgbody is finished, then go back to header reader
     * Warning: m_header has already been init(cleared)
     * RecvNode<std::string>: only create a Header
     */
    m_recv_buffer.reset(new Recv(ByteOrderConverter{}));

    boost::asio::async_read(
        session->s_socket,
        boost::asio::buffer(m_recv_buffer->get_header_base(),
                            m_recv_buffer->get_header_length()),
        std::bind(&Session::handle_header, this, session, std::placeholders::_1,
                  std::placeholders::_2));
  } catch (const std::exception &e) {
    spdlog::error("[{}] handle_msgbody {}",
                  ServerConfig::get_instance()->GrpcServerName, e.what());
  }
}

bool Session::checkDeferredTermination() {

  auto state = m_state.load();
  if (state == SessionState::LogoutPending || state == SessionState::Kicked) {
    spdlog::info("[{}] Session {} is terminating, skipping async_write.",
                 ServerConfig::get_instance()->GrpcServerName, s_session_id);

    if (m_finalSendCompleteHandler) {
      m_state.store(SessionState::Terminated);
      m_finalSendCompleteHandler(); // execute defer handler
      m_finalSendCompleteHandler = nullptr;
    }
    return true;
  }
  return false;
}

const std::string &Session::get_user_uuid() const { return s_uuid; }

const std::string &Session::get_session_id() const { return s_session_id; }

void Session::markAsDeferredTerminated(std::function<void()> &&callable) {

  m_state = SessionState::LogoutPending;
  m_finalSendCompleteHandler = std::move(callable);

  // SessionState expected = SessionState::Alive;
  // if (m_state.compare_exchange_strong(expected, SessionState::LogoutPending))
  // {
  //           m_finalSendCompleteHandler = std::move(callable);
  // }
  // else {
  //           spdlog::warn("[{}] Session::markAsDeferredTerminated() called in
  //           wrong state: {}",
  //                     ServerConfig::get_instance()->GrpcServerName,
  //                     static_cast<int>(expected));
  // }
}

void Session::sendOfflineMessage() {
  boost::json::object logout;

  logout["error"] = static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS);
  logout["uuid"] = get_user_uuid();

  sendMessage(ServiceType::SERVICE_LOGOUTRESPONSE,
              boost::json::serialize(logout), shared_from_this());

  /*Now we have to remove this, because it might causing other issue
   * it has to be deployed seperatly, and ALSO IT SHOULD NOT BE DEPLOYED
   *  IN TRAVERSAL SCENAIRO
   * UserManager::get_instance()->moveUserToTerminationZone(get_user_uuid());
   */
}

void Session::decrementConnection() {
  RedisRAII raii;

  auto get_distributed_lock =
      raii->get()->acquire(ServerConfig::get_instance()->GrpcServerName, 10, 10,
                           redis::TimeUnit::Milliseconds);

  if (!get_distributed_lock.has_value()) {
    spdlog::error(
        "[{}] Acquire Distributed-Lock In DecrementConnection Failed!",
        ServerConfig::get_instance()->GrpcServerName);
    return;
  }

  spdlog::info(
      "[{}] Acquire Distributed-Lock In DecrementConnection Successful!",
      ServerConfig::get_instance()->GrpcServerName);

  /*try to acquire value from redis*/
  std::optional<std::string> counter = raii->get()->getValueFromHash(
      redis_server_login, ServerConfig::get_instance()->GrpcServerName);

  std::size_t new_number(0);

  /* redis has this value then read it from redis*/
  if (counter.has_value()) {
    new_number = tools::string_to_value<std::size_t>(counter.value()).value();
  }

  // new_number != 0
  if (new_number > 0) {
    /*decerment and set value to hash by using HSET*/
    if (!raii->get()->setValue2Hash(
            redis_server_login, ServerConfig::get_instance()->GrpcServerName,
            std::to_string(--new_number))) {

      spdlog::error("[{}] Client Number Can Not Be Written To Redis Cache! "
                    "Error Occured!",
                    ServerConfig::get_instance()->GrpcServerName);
    }
  }

  // release lock
  raii->get()->release(ServerConfig::get_instance()->GrpcServerName,
                       get_distributed_lock.value());

  /*store this user belonged server into redis*/
  spdlog::info("[{}] Now {} Client Has Connected To Current Server",
               ServerConfig::get_instance()->GrpcServerName, new_number);
}
