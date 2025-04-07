#pragma once
#ifndef _DISTRIBUTED_SPIN_REDIS_LOCK_HPP_
#define _DISTRIBUTED_SPIN_REDIS_LOCK_HPP_
#include <memory>
#include <sstream>
#include <singleton/singleton.hpp>
#include <redis/RedisContextRAII.hpp>

namespace lock {

          class DistributedSpinRedisLock :public Singleton<DistributedSpinRedisLock> {

                    using context = redis::RedisContext;
                    friend class Singleton<DistributedSpinRedisLock>;

                    DistributedSpinRedisLock();

          public:
                    virtual ~DistributedSpinRedisLock();

          public:
                    std::optional<std::string> 
                    acquire(const std::string& lockName,
                                 const std::string& uuid,
                                 const std::size_t waitTime,
                                 const std::size_t EXPX,
                                 redis::TimeUnit unit = redis::TimeUnit::Seconds);

                    bool release();

          private:
                    bool m_error = false;
                    std::ostringstream m_threadId;
                    std::string m_lockName;
                    std::string m_uuid;
                    std::unique_ptr<context> m_stub;
          };
}

#endif  //_DISTRIBUTED_SPIN_REDIS_LOCK_HPP_