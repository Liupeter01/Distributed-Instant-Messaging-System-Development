#include <server/AsyncServer.hpp>
#include <server/UserManager.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/spdlog.h>

AsyncServer::AsyncServer(boost::asio::io_context &_ioc, unsigned short port)
    : m_ioc(_ioc),
      m_acceptor(_ioc, boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::address_v4::any(), port)) {
  spdlog::info("Chatting Server activated, listen on port {}", port);
}

AsyncServer::~AsyncServer() {
  spdlog::critical("Chatting Sever Shutting Down!");
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
  } else {
    spdlog::info("[Session = {}]Chatting Server Accept failed",
                 session->s_session_id);
    this->terminateConnection(session->s_session_id);
  }
  this->startAccept();
}

void AsyncServer::terminateConnection(const std::string & user_uuid) {

  /*remove the bind of uuid and session inside UserManager*/
  UserManager::get_instance()->removeUsrSession(user_uuid);
}
