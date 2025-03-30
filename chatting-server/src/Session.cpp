#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <config/ServerConfig.hpp>
#include <handler/SyncLogic.hpp>
#include <server/AsyncServer.hpp>
#include <server/Session.hpp>
#include <spdlog/spdlog.h>

Session::Session(boost::asio::io_context &_ioc, AsyncServer *my_gate)
    : s_closed(false), s_socket(_ioc), s_gate(my_gate),
      m_recv_buffer(std::make_unique<Recv>([](auto x) {
        return boost::asio::detail::socket_ops::network_to_host_short(x);
      })) /*init header buffer init*/
{
  /*generate the session id*/
  boost::uuids::uuid uuid_gen = boost::uuids::random_generator()();
  this->s_session_id = boost::uuids::to_string(uuid_gen);
}

Session::~Session() {
  if (!s_closed) {
    closeSession();
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
    spdlog::error("{}", e.what());
  }
}

void Session::setUUID(const std::string &uuid) { s_uuid = uuid; }

void Session::closeSession() {
  s_closed = true;
  s_socket.close();
}

void Session::sendMessage(ServiceType srv_type, const std::string &message) {
  try {
    std::lock_guard<std::mutex> _lckg(m_mtx);
    if (m_send_queue.size() >
        ServerConfig::get_instance()->ChattingServerQueueSize) {
      spdlog::warn("Client [UUID = {}] Sending Queue is full!");
      return;
    }

    /*inside SendNode ctor, temporary must be modifiable*/
    std::string temporary = message;

    m_send_queue.push(std::make_unique<Send>(
        static_cast<uint16_t>(srv_type), temporary, [](auto x) {
          return boost::asio::detail::socket_ops::host_to_network_short(x);
        }));

    /*currently, there is no task inside queue*/
    if (!m_send_queue.empty()) {
      auto &front = m_send_queue.front();
      boost::asio::async_write(s_socket,
                               boost::asio::buffer(front->get_header_base(),
                                                   front->get_full_length()),
                               std::bind(&Session::handle_write, this,
                                         shared_from_this(),
                                         std::placeholders::_1));
    }
  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
}

void Session::handle_write(std::shared_ptr<Session> session,
                           boost::system::error_code ec) {
  try {
    /*error occured*/
    if (ec) {
      session->s_gate->terminateConnection(session->s_session_id);
      spdlog::warn(
          "[Client UUID = {}]: Session {} Exit Anomaly, Error message {}",
          session->s_uuid, session->s_session_id, ec.message());
      return;
    }
    std::lock_guard<std::mutex> _lckg(m_mtx);

    /*when the first element was processed, then pop it out*/
    m_send_queue.pop();

    /*till there is no element inside queue*/
    if (!m_send_queue.empty()) {
      auto &front = m_send_queue.front();
      boost::asio::async_write(s_socket,
                               boost::asio::buffer(front->get_header_base(),
                                                   front->get_full_length()),
                               std::bind(&Session::handle_write, this,
                                         shared_from_this(),
                                         std::placeholders::_1));
    }
  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
}

void Session::handle_header(std::shared_ptr<Session> session,
                            boost::system::error_code ec,
                            std::size_t bytes_transferred) {
  try {
    /*error occured*/
    if (ec) {
      session->s_gate->terminateConnection(session->s_session_id);
      session->closeSession();
      spdlog::warn(
          "Client [Session = {}] Header Error: Exit Anomaly, Error message {}",
          session->s_session_id, ec.message());
      return;
    }

    /*update remainning data to acquire*/
    m_recv_buffer->update_pointer_pos(bytes_transferred);

    /*current, we didn't get the full size of the header*/
    if (m_recv_buffer->check_header_remaining()) {
      session->s_gate->terminateConnection(session->s_session_id);
      session->closeSession();
      spdlog::warn("Client [Session = {}] Header Error: Exit Due To Transfer "
                   "Issue, Only {} Bytes Received!",
                   session->s_session_id, bytes_transferred);
      return;
    }

    /*
     * get msg_id and msg_length
     * and change the network sequence and convert network ----> host
     */
    std::optional<uint16_t> id = m_recv_buffer->get_id();
    if (!id.has_value()) {
      session->s_gate->terminateConnection(session->s_session_id);
      session->closeSession();
      spdlog::warn("Client [Session = {}] Header Error: Invalid ID!",
                   session->s_session_id);
      return;
    }

    uint16_t msg_id = id.value();
    if (msg_id >= static_cast<uint16_t>(ServiceType::SERVICE_UNKNOWN)) {
      session->s_gate->terminateConnection(session->s_session_id);
      session->closeSession();
      spdlog::warn("Client [Session = {}] Header Error: Exit Due To Invalid "
                   "Service ID {}!",
                   session->s_session_id, msg_id);
      return;
    }

    std::optional<uint16_t> length = m_recv_buffer->get_length();
    if (!length.has_value()) {
      session->s_gate->terminateConnection(session->s_session_id);
      session->closeSession();
      spdlog::warn("Client [Session = {}] Header Error: Invalid Length!",
                   session->s_session_id);
      return;
    }

    uint16_t msg_length = length.value();

    if (msg_length > MAX_LENGTH) {
      session->s_gate->terminateConnection(session->s_session_id);
      session->closeSession();
      spdlog::warn(
          "Client [Session = {}] Header Error: Exit Due To Invalid Data "
          "Length, {} Bytes Received!",
          session->s_session_id, msg_length);
      return;
    }

    /*for the safty, we have to reset the MsgNode first to prevent memory leak*/
    boost::asio::async_read(
        session->s_socket,
        boost::asio::buffer(m_recv_buffer->get_body_base(), msg_length),
        std::bind(&Session::handle_msgbody, this, session,
                  std::placeholders::_1, std::placeholders::_2));
  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
}

void Session::handle_msgbody(std::shared_ptr<Session> session,
                             boost::system::error_code ec,
                             std::size_t bytes_transferred) {
  try {
    /*error occured*/
    if (ec) {
      session->s_gate->terminateConnection(session->s_session_id);
      spdlog::warn(
          "Client [Session = {}] Body Error: Exit Anomaly, Error message {}",
          session->s_session_id, ec.message());
      return;
    }

    /*update remainning data to acquire*/
    m_recv_buffer->update_pointer_pos(bytes_transferred);

    /*data is not fully received*/
    if (m_recv_buffer->check_body_remaining()) {
      session->s_gate->terminateConnection(session->s_session_id);
      spdlog::warn(
          "Client [Session = {}] Body Error: Exit Anomaly, Not Fully Recevied",
          session->s_session_id);
      return;
    }

    /*release owner ship of the data, you must release in another unique_ptr*/
    RecvPtr recv(m_recv_buffer.release());

    /*send the received data to SyncLogic to process it */
    SyncLogic::get_instance()->commit(std::make_pair(session, std::move(recv)));

    /*
     * if handle_msgbody is finished, then go back to header reader
     * Warning: m_header has already been init(cleared)
     * RecvNode<std::string>: only create a Header
     */
    m_recv_buffer.reset(new Recv([](auto x) {
      return boost::asio::detail::socket_ops::network_to_host_short(x);
    }));

    boost::asio::async_read(
        session->s_socket,
        boost::asio::buffer(m_recv_buffer->get_header_base(),
                            m_recv_buffer->get_header_length()),
        std::bind(&Session::handle_header, this, session, std::placeholders::_1,
                  std::placeholders::_2));
  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
  }
}
