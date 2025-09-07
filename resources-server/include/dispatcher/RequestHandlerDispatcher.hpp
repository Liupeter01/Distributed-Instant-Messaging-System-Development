#pragma once
#ifndef _REQUEST_HANDLER_DISPATCHER_HPP_
#define _REQUEST_HANDLER_DISPATCHER_HPP_
#include <string>
#include <optional>
#include <string_view>
#include <spdlog/spdlog.h>
#include <server/Session.hpp>
#include <buffer/MsgNode.hpp>
#include <singleton/singleton.hpp>
#include <tbb/concurrent_vector.h>
#include <config/ServerConfig.hpp>
#include <handler/RequestHandlerNode.hpp>

namespace dispatcher {

class RequestHandlerDispatcher : public Singleton<RequestHandlerDispatcher> {

  friend class Singleton<RequestHandlerDispatcher>;
  using SessionPtr = std::shared_ptr<Session>;
  using NodePtr = std::unique_ptr<RecvNode<std::string, ByteOrderConverter>>;
  using pair = std::pair<SessionPtr, NodePtr>;
  using ContainerType =
      tbb::concurrent_vector<std::shared_ptr<handler::RequestHandlerNode>>;

public:
          virtual ~RequestHandlerDispatcher() {
                    shutdown();
          }
  void commit(pair&& recv_node, [[maybe_unused]] SessionPtr live_extend) {

            if (!recv_node.first) return;
            auto session_id = recv_node.first->get_session_id();

            // if opt has value then it could be executed by this if condition
            auto temp = dispatch_to_node(session_id);
            if (!temp.has_value()) {
                      spdlog::error("[{}]: Dispatcher Error While Handling Session: {}",
                                ServerConfig::get_instance()->GrpcServerName,
                                session_id);
                      return;
            }

            auto node = temp.value();
            node->commit(std::move(recv_node), live_extend);

            //spdlog::info("[{}]: Dispatcher Request To Handler Node {} Successfully!",
            //          ServerConfig::get_instance()->GrpcServerName,
            //          node->getId());
  }

protected:
          std::size_t hash_to_index(std::string_view session_id) const {
                    return m_convertor(session_id) % m_nodes.size();
          }

private:
          RequestHandlerDispatcher()
                    : RequestHandlerDispatcher(std::thread::hardware_concurrency() < 2
                              ? 2
                              : std::thread::hardware_concurrency()) {
          }
          RequestHandlerDispatcher(std::size_t threads)
                    : m_convertor() {
                    for (std::size_t thread = 0; thread < threads; ++thread) {
                              m_nodes.emplace_back(std::make_shared<handler::RequestHandlerNode>(thread));
                    }
          }
          void shutdown() {
                    std::for_each(m_nodes.begin(), m_nodes.end(),
                              [](std::shared_ptr<handler::RequestHandlerNode>& node) {
                                        node->shutdown();
                              });
          }
  ContainerType::iterator dispatch_to_iterator(std::string_view session_id) {
            if (m_nodes.empty()) {
                      return m_nodes.end(); // or throw
            }

            auto it = m_nodes.begin() + hash_to_index(session_id);
            if (it != m_nodes.end()) {
                      return it;
            }
            return m_nodes.end();
  }
  std::optional<std::shared_ptr<handler::RequestHandlerNode>>
      dispatch_to_node(std::string_view session_id) {
      if (m_nodes.empty())
          return std::nullopt;

      try {
          return m_nodes.at(hash_to_index(session_id));
      }
      catch (const std::exception& e) {
          spdlog::error(
              "[{}]: Retrieve Request Handler Node Error, Reason:{}",
                    ServerConfig::get_instance()->GrpcServerName,
              e.what());
      }
      return std::nullopt;
  }

private:
  std::hash<std::string_view> m_convertor;
  ContainerType m_nodes;
};
} // namespace dispatcher

#endif
