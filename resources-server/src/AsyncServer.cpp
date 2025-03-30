#include <server/AsyncServer.hpp>
#include <server/UserManager.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/spdlog.h>

AsyncServer::AsyncServer(boost::asio::io_context &_ioc, unsigned short port)
    : m_ioc(_ioc),
      m_acceptor(_ioc, boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::address_v4::any(), port)) {
  spdlog::info("Resources Server activated, listen on port {}", port);
}

AsyncServer::~AsyncServer() {
  spdlog::critical("Resources Sever Shutting Down!");
}

void AsyncServer::startAccept() {
  auto &ioc = IOServicePool::get_instance()->getIOServiceContext();
  std::shared_ptr<Session> session = std::make_shared<Session>(ioc, this);

  m_acceptor.async_accept(
      session->s_socket,
      std::bind(&AsyncServer::handleAccept, this, session,
                std::placeholders::_1) /*extend the life length of the session*/
  );
}

void AsyncServer::handleAccept(std::shared_ptr<Session> session,
                               boost::system::error_code ec) {
  if (!ec) {
    /*start session read and write function*/
    session->startSession();

    std::lock_guard<std::mutex> _lckg(m_mtx);
    m_sessions.insert(std::make_pair(session->s_session_id, session));
  } else {
    spdlog::info("[Session = {}] Resources Server Accept failed",
                 session->s_session_id);
    this->terminateConnection(session->s_session_id);
  }
  this->startAccept();
}

void AsyncServer::terminateConnection(const std::string &session_id) {
  std::lock_guard<std::mutex> _lckg(m_mtx);
  auto session = this->m_sessions.find(session_id);

  /*we found nothing*/
  if (session == this->m_sessions.end()) {
    spdlog::warn("[Session = {}] Session ID Not Found!", session_id);
    return;
  }

  /*remove the bind of uuid and session inside UserManager*/
  UserManager::get_instance()->removeUsrSession(session->second->s_uuid);
  session->second->closeSession(); // close socket connection
  this->m_sessions.erase(session); // erase it from map
}
