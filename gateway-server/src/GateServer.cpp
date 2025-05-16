#include <http/HttpConnection.hpp>
#include <server/GateServer.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/spdlog.h>

GateServer::GateServer(boost::asio::io_context &_ioc, unsigned short port)
    : m_ioc(_ioc),
      m_acceptor(_ioc, boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::address_v4::any(), port)) {
  spdlog::info("[Gateway Server]: Server Activated! Listening On Port {}",
               port);
  this->serverStart();
}

GateServer::~GateServer() {
  spdlog::critical("[Gateway Server]: Server Shutting Down!");
}

void GateServer::serverStart() {
  boost::asio::io_context &ioc =
      IOServicePool::get_instance()->getIOServiceContext(); // get ioc
  std::shared_ptr<Session<GateServer>> session =
      std::make_shared<Session<GateServer>>(ioc, this);

  this->m_acceptor.async_accept(session->s_socket,
                                std::bind(&GateServer::handleAccept, this,
                                          session, std::placeholders::_1));
}

void GateServer::handleAccept(std::shared_ptr<Session<GateServer>> session,
                              boost::system::error_code ec) {
  if (!ec) {

    /*establish HTTPConnection to handle socket*/
    std::shared_ptr<HTTPConnection> http(
        std::make_shared<HTTPConnection>(session->s_socket));
    http->start_service();

    /*add to session pool*/
    std::lock_guard<std::mutex> _lckg(m_mtx);
    this->m_sessions.insert(std::make_pair(session->s_uuid, session));

  } else /*error occured!*/
  {
    spdlog::warn("[Gateway Server]: Accept {} failed", session->s_uuid);
    this->terminateSession(session->s_uuid);
  }

  /*accept connection recursively*/
  this->serverStart();
}

void GateServer::terminateSession(const std::string &uuid) {
  /*then remove it from the map, we should also lock it first*/
  std::lock_guard<std::mutex> _lckg(m_mtx);

  auto it = this->m_sessions.find(uuid);

  /*add safety consideration*/
  if (it != this->m_sessions.end()) {
    it->second->closeSession();
    this->m_sessions.erase(uuid);
  }
}
