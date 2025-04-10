#pragma once
#ifndef _REQUEST_HANDLER_NODE_HPP_
#define _REQUEST_HANDLER_NODE_HPP_
#include <atomic>
#include <buffer/MsgNode.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <network/def.hpp>
#include <optional>
#include <queue>
#include <server/Session.hpp>
#include <thread>
#include <unordered_map>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <redis/RedisManager.hpp>
#include <service/ConnectionPool.hpp>

namespace handler {

class RequestHandlerNode {
public:
  using Convertor = std::function<unsigned short(unsigned short)>;
  using SessionPtr = std::shared_ptr<Session>;
  using NodePtr = std::unique_ptr<RecvNode<std::string, ByteOrderConverter>>;
  using pair = std::pair<SessionPtr, NodePtr>;
  using CallbackFunc =
      std::function<void(ServiceType, std::shared_ptr<Session>, NodePtr)>;

  using  RedisRAII = connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>;

public:
  RequestHandlerNode();
  RequestHandlerNode(const std::size_t id);
  ~RequestHandlerNode();
  void shutdown();
  void setHandlerId(const std::size_t id);
  const std::size_t getId() const;

  void commit(pair recv_node, [[maybe_unused]] SessionPtr live_extend);

  static bool parseJson(std::shared_ptr<Session> session, NodePtr& recv,
            boost::json::object& src_obj);

  static void generateErrorMessage(const std::string &log, ServiceType type,
                                   ServiceStatus status, SessionPtr conn);

private:
  /*SyncLogic Class Operations*/
  void processing();
  void registerCallbacks();
  void execute(pair &&node);

  /*client enter current server*/
  void incrementConnection();
  void decrementConnection();

  /*store this user belonged server into redis*/
  bool tagCurrentUser(const std::string &uuid);

  /*delete user belonged server in redis*/
  bool untagCurrentUser(const std::string &uuid);

protected:
  void handlingLogin(ServiceType srv_type, std::shared_ptr<Session> session,
                     NodePtr recv);

  void handlingLogout(ServiceType srv_type, std::shared_ptr<Session> session,
                      NodePtr recv);

  void handlingFileUploading(ServiceType srv_type,
                             std::shared_ptr<Session> session, NodePtr recv);

public:
  /*redis*/
  static std::string redis_server_login;

  /*store user base info in redis*/
  static std::string user_prefix;

  /*store the server name that this user belongs to*/
  static std::string server_prefix;

private:
  std::size_t handler_id;
  std::atomic<bool> m_stop;

  /*working thread, handling commited request*/
  std::thread m_working;

  /*mutex & cv => thread safety*/
  std::mutex m_mtx;
  std::condition_variable m_cv;

  /*user commit data to the queue*/
  std::queue<pair> m_queue;

  /*callback list*/
  std::unordered_map<ServiceType, CallbackFunc> m_callbacks;
};
} // namespace handler

#endif //_SYNCLOGIC_HPP_
