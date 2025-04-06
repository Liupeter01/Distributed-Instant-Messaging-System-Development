#include <algorithm>
#include <dispatcher/RequestHandlerDispatcher.hpp>
#include <spdlog/spdlog.h>

dispatcher::RequestHandlerDispatcher::RequestHandlerDispatcher()
    : RequestHandlerDispatcher(std::thread::hardware_concurrency() < 2
                                   ? 2
                                   : std::thread::hardware_concurrency()) {}

dispatcher::RequestHandlerDispatcher::RequestHandlerDispatcher(
    std::size_t threads)
    : m_convertor() {
  for (std::size_t thread = 0; thread < threads; ++thread) {
    m_nodes.emplace_back(std::make_shared<handler::RequestHandlerNode>(thread));
  }
}

dispatcher::RequestHandlerDispatcher::~RequestHandlerDispatcher() {
  shutdown();
}

void dispatcher::RequestHandlerDispatcher::shutdown() {
  std::for_each(m_nodes.begin(), m_nodes.end(),
                [](std::shared_ptr<handler::RequestHandlerNode> &node) {
                  node->shutdown();
                });
}

void dispatcher::RequestHandlerDispatcher::commit(
    pair &&recv_node, [[maybe_unused]] SessionPtr live_extend) {

  auto session_id = recv_node.first->get_session_id();

  // if opt has value then it could be executed by this if condition
  if (auto opt = dispatch_to_node(session_id); opt) {
    (*opt)->commit(std::move(recv_node), live_extend);
    spdlog::info("[Resources Server]: Dispatcher Network Request To Node {} "
                 "Successfully!",
                 (*opt)->getId());
    return;
  }

  spdlog::error(
      "[Resources Server]: Dispatcher Error While Handling Session: {}",
      session_id);
}

std::size_t dispatcher::RequestHandlerDispatcher::hash_to_index(
    std::string_view session_id) const {
  return m_convertor(session_id) % m_nodes.size();
}

dispatcher::RequestHandlerDispatcher::ContainerType::iterator
dispatcher::RequestHandlerDispatcher::dispatch_to_iterator(
    std::string_view session_id) {
  if (m_nodes.empty()) {
    return m_nodes.end(); // or throw
  }

  return m_nodes.begin() + hash_to_index(session_id);
}

std::optional<std::shared_ptr<handler::RequestHandlerNode>>
dispatcher::RequestHandlerDispatcher::dispatch_to_node(
    std::string_view session_id) {
  if (m_nodes.empty())
    return std::nullopt;
  return m_nodes[m_convertor(session_id) % m_nodes.size()];
}
