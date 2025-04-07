#include <spdlog/spdlog.h>
#include <config/ServerConfig.hpp>
#include <spinlock/DistributedSpinRedisLock.hpp>

lock::DistributedSpinRedisLock::DistributedSpinRedisLock() {

          spdlog::info("[DistributedSpinRedisLock]: Connecting to Redis service ip: {0}, port: {1}",
                    ServerConfig::get_instance()->Redis_ip_addr.c_str(),
                    ServerConfig::get_instance()->Redis_port);

          m_stub = std::make_unique<context>(
                    ServerConfig::get_instance()->Redis_ip_addr,
                    ServerConfig::get_instance()->Redis_port,
                    ServerConfig::get_instance()->Redis_passwd);
}

lock::DistributedSpinRedisLock::~DistributedSpinRedisLock() {

          //if there is no any error happened during acquire lock phase
          if (!m_error) {
                    //Release Redis Distributed Lock!
                    this->release();
          }
}

std::optional<std::string>
lock::DistributedSpinRedisLock::acquire(const std::string& lockName,
          const std::string& uuid,
          const std::size_t waitTime,
          const std::size_t EXPX,
          redis::TimeUnit unit) {

          m_threadId.clear();
          m_threadId <<  std::this_thread::get_id();

          if (auto opt = m_stub->acquire(lockName, uuid, waitTime, EXPX, unit); opt) {

                    spdlog::info("[DistributedSpinRedisLock]: SubThread {} Acquire Distributed Lock Successfully",
                              m_threadId.str());

                    m_lockName = lockName;
                    m_uuid = uuid;
                    return *opt;
          }

          spdlog::warn("[DistributedSpinRedisLock]: SubThread {} Failed to Acquire Distributed Lock",
                    m_threadId.str());

          m_error = true;
          return std::nullopt;
}

bool lock::DistributedSpinRedisLock::release() {
          return m_stub->release(m_lockName, m_uuid);
}