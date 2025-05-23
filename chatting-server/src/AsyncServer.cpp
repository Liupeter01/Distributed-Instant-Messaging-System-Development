#include <config/ServerConfig.hpp>
#include <server/AsyncServer.hpp>
#include <server/UserManager.hpp>
#include <service/IOServicePool.hpp>
#include <spdlog/spdlog.h>

AsyncServer::AsyncServer(boost::asio::io_context &_ioc, unsigned short port)
    : m_ioc(_ioc),
      m_timer(_ioc,
              boost::asio::chrono::seconds(
                  ServerConfig::get_instance()
                      ->heart_beat_timeout)) /*bind a scheduler for timer!*/
      ,
      m_acceptor(_ioc, boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::address_v4::any(), port)) {

  /*remove timercallback in the ctor function
   *because memory are not ready YET
   * so,   registerTimerCallback() has to be removed!!!!
   */

  spdlog::info("[{}] Server Activated, Listen On Port {}",
               ServerConfig::get_instance()->GrpcServerName, port);
}

AsyncServer::~AsyncServer() {}

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
    spdlog::warn("[{}] Client Session {} UUID {} Accept failed! "
                 "Error message {}",
                 ServerConfig::get_instance()->GrpcServerName,
                 session->s_session_id, session->s_uuid, ec.message());

    this->terminateConnection(session->get_user_uuid());
  }
  this->startAccept();
}

void AsyncServer::startTimer() {

  // when developer cancel timer, timer will be deployed again!
  //  if "this" is destroyed, then it might causing errors!!!
  //  extend the life length of the structure
  auto self = shared_from_this();
  m_timer.async_wait(
      [this, self](boost::system::error_code ec) { heartBeatEvent(ec); });
}

void AsyncServer::stopTimer() {
  /*cancel timer event and remove tasks from io_context queue
   *but it can not ensure that the timer event has removed from the queue
   *we should stop it before deploying dtor function*/
  m_timer.cancel();
}

void AsyncServer::shutdown() {

          spdlog::info("[{}] Start Kicking All The Clients Off The Server, Please Stand By ...",
                    ServerConfig::get_instance()->GrpcServerName);

          //send offline message to all the clients
          UserManager::get_instance()->teminate();
}

void AsyncServer::heartBeatEvent(const boost::system::error_code &ec) {

  // Error's Occured!
  if (ec) {
    spdlog::error("[{}] Executing HeartBeat Purge Program Error Occured! Error "
                  "Code is: {}",
                  ServerConfig::get_instance()->GrpcServerName, ec.message());

    return;
  }

  spdlog::info(
      "[{}] Executing HeartBeat Purge Program, Kill Zombie Connections!",
      ServerConfig::get_instance()->GrpcServerName);

  std::time_t now = std::time(nullptr);

  /*only record "dead" session's uuid, and we deal with them later*/
  std::vector<std::string> to_be_terminated;

  // copy original session to a duplicate one
  UserManager::ContainerType &lists =
      UserManager::get_instance()->m_uuid2Session;

  // record valid connection amount
  std::size_t session_counter{0};

  for (auto &client : lists) {
    // check if this user already timeout!
    if (!client.second->isSessionTimeout(now)) {
      // not timeout, continue
      ++session_counter;
      continue;
    }

    // Ask the client to be offlined, and move it to waitingToBeClosed queue
    client.second->sendOfflineMessage();
    client.second->removeRedisCache(client.second->get_user_uuid(),
                                    client.second->get_session_id());

    // collect expired client info, and we process them later!
    to_be_terminated.push_back(client.first);
  }

  /*now, we move them to temination list*/
  for (const auto &gg : to_be_terminated) {
    UserManager::get_instance()->moveUserToTerminationZone(gg);
    UserManager::get_instance()->removeUsrSession(gg);
  }

  // re-register timer event
  m_timer.expires_after(boost::asio::chrono::seconds(
      ServerConfig::get_instance()->heart_beat_timeout));
  startTimer();
}

void AsyncServer::moveUserToTerminationZone(const std::string &user_uuid) {
  UserManager::get_instance()->moveUserToTerminationZone(user_uuid);
}

void AsyncServer::terminateConnection(const std::string &user_uuid) {

  /*remove the bind of uuid and session inside UserManager*/
  UserManager::get_instance()->removeUsrSession(user_uuid);
}

void AsyncServer::terminateConnection(const std::string &user_uuid,
                                      const std::string &expected_session_id) {
  /*remove the bind of uuid and session inside UserManager*/
  UserManager::get_instance()->removeUsrSession(user_uuid, expected_session_id);
}
