#pragma once
#ifndef _REQUEST_HANDLER_DISPATCHER_HPP_
#define _REQUEST_HANDLER_DISPATCHER_HPP_
#include <buffer/MsgNode.hpp>
#include <handler/RequestHandlerNode.hpp>
#include <optional>
#include <singleton/singleton.hpp>
#include <string>
#include <string_view>
#include <tbb/concurrent_vector.h>

namespace dispatcher {

class RequestHandlerDispatcher : public Singleton<RequestHandlerDispatcher> {

  friend class Singleton<RequestHandlerDispatcher>;
  using SessionPtr = std::shared_ptr<Session>;
  using NodePtr = std::unique_ptr<RecvNode<std::string, ByteOrderConverter>>;
  using pair = std::pair<SessionPtr, NodePtr>;
  using ContainerType =
      tbb::concurrent_vector<std::shared_ptr<handler::RequestHandlerNode>>;

public:
  virtual ~RequestHandlerDispatcher();
  void commit(pair &&recv_node, [[maybe_unused]] SessionPtr live_extend);

protected:
  std::size_t hash_to_index(std::string_view session_id) const;

private:
  RequestHandlerDispatcher();
  RequestHandlerDispatcher(std::size_t threads);
  void shutdown();
  ContainerType::iterator dispatch_to_iterator(std::string_view session_id);
  std::optional<std::shared_ptr<handler::RequestHandlerNode>>
  dispatch_to_node(std::string_view session_id);

private:
  std::hash<std::string_view> m_convertor;
  ContainerType m_nodes;
};
} // namespace dispatcher

#endif
