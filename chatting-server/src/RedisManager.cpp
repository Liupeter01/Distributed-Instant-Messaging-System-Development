#include <chrono>
#include <config/ServerConfig.hpp>
#include <redis/RedisContextRAII.hpp>
#include <redis/RedisManager.hpp>
#include <spdlog/spdlog.h>

redis::RedisConnectionPool::RedisConnectionPool() noexcept
    : RedisConnectionPool(ServerConfig::get_instance()->Redis_timeout,
                          ServerConfig::get_instance()->Redis_ip_addr,
                          ServerConfig::get_instance()->Redis_passwd,
                          ServerConfig::get_instance()->Redis_port) {
  spdlog::info("[Redis Connector]: Connecting to Redis Server: {0}, port: {1}",
               ServerConfig::get_instance()->Redis_ip_addr.c_str(),
               ServerConfig::get_instance()->Redis_port);
}

redis::RedisConnectionPool::RedisConnectionPool(
    const std::size_t _timeout, const std::string &_ip,
    const std::string &_passwd, const unsigned short _port) noexcept

    : m_timeout(_timeout), m_ip(_ip), m_passwd(_passwd), m_port(_port) {

  for (std::size_t i = 0; i < m_queue_size; ++i) {
    [[maybe_unused]] auto status = connector(m_ip, m_passwd, m_port);
  }

  m_RRThread = std::thread([this]() {
    thread_local std::size_t counter{0};
    spdlog::info("[Redis HeartBeat Check]: Timeout Setting {}s", m_timeout);

    while (m_stop) {

      if (counter == m_timeout) {
        roundRobinChecking();
        counter = 0; // reset
      }

      /*suspend this thread by timeout setting*/
      std::this_thread::sleep_for(std::chrono::seconds(1));
      counter++;
    }
  });
  m_RRThread.detach();
}

void redis::RedisConnectionPool::roundRobinChecking() {

  if (m_stop)
    return;

  std::size_t fail_count = 0;
  std::size_t expectedStubs{0}, currentStubs{0};
  auto currentTimeStamp = std::chrono::steady_clock::now();

  // get target queue size first, we need to know how many stubs are instead the
  // queue
  {
    std::lock_guard<std::mutex> _lckg(m_mtx);
    expectedStubs = m_stub_queue.size();
  }

  for (; !expectedStubs && currentStubs < expectedStubs; currentStubs++) {

    {
      // sometimes. m_stub_queue might be empty;
      std::lock_guard<std::mutex> _lckg(m_mtx);
      if (m_stub_queue.empty()) {
        break;
      }
    }

    // get stub from the queue
    connection::ConnectionRAII<redis::RedisConnectionPool, redis::RedisContext>
        instance;

    if (std::chrono::duration_cast<std::chrono::seconds>(
            currentTimeStamp - instance->get()->last_operation_time) <
        std::chrono::seconds(5)) {
      continue;
    }

    try {
      /*execute timeout checking, if there is sth wrong , then throw exceptionn
       * and re-create connction*/
      if (!instance->get()->heartBeat()) [[unlikely]]
        throw std::runtime_error("Check Timeout Failed!");

      /*update current operation time!*/
      instance->get()->last_operation_time = currentTimeStamp;
    } catch (const std::exception &e) {
      /*checktimeout error, but we will handle restart later*/
      spdlog::warn("[Redis DataBase]: Error = {} Restarting Connection...",
                   e.what());

      // disable RAII feature to return this item back to the pool
      instance.invalidate();

      fail_count++; // record failed time!
    }
  }

  // handle failed events, and try to reconnect
  while (fail_count > 0) {
    if (!connector(m_ip, m_passwd, m_port)) [[unlikely]] {
      return;
    }
    fail_count--;
  }
}

bool redis::RedisConnectionPool::connector(const std::string &_ip,
                                           const std::string &_passwd,
                                           const unsigned short _port) {

  auto currentTimeStamp = std::chrono::steady_clock::now();

  try {
    auto new_item = std::make_unique<redis::RedisContext>(_ip, _port, _passwd);
    new_item->last_operation_time = currentTimeStamp;

    //We have to do auth, to check whether password is correct or not!
    if (!new_item->checkAuth(m_passwd)) {
              new_item.reset();
              throw std::runtime_error("Redis Auth Failed!")
    }

    {
      std::lock_guard<std::mutex> _lckg(m_mtx);
      m_stub_queue.push(std::move(new_item));
    }
    return true;
  } catch (const std::exception &e) {
    spdlog::warn("[Redis Connector]: Error = {}", e.what());
  }
  return false;
}
