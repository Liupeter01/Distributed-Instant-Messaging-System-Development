#include <chrono>
#include <redis/RedisContextRAII.hpp>
#include <redis/RedisReplyRAII.hpp>
#include <spdlog/spdlog.h>

redis::RedisContext::RedisContext() noexcept
    : m_valid(false), m_redisContext(nullptr) {}

redis::RedisContext::RedisContext(const std::string &ip, unsigned short port,
                                  const std::string &password) noexcept
    : m_valid(false), m_redisContext(redisConnect(ip.c_str(), port)) {
  /*error occured*/
  if (!checkError()) {
    m_redisContext.reset();
  } else {
    checkAuth(password);
    spdlog::info("[Redis]: Connection to Redis Server Successful!");
  }
}

bool redis::RedisContext::isValid() { return m_valid; }

bool redis::RedisContext::setValue(const std::string &key,
                                   const std::string &value) {

  if (key.empty()) {
    return false;
  }
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("SET %s %s"),
                                              key.c_str(), value.c_str());
  if (status) {
    spdlog::info("[Redis]: Execute command [ SET key = {0}, value = {1}] successfully!",
                 key.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::setValue2Hash(const std::string &key,
                                        const std::string &field,
                                        const std::string &value) {

  if (key.empty()) {
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status =
      m_replyDelegate->redisCommand(*this, std::string("HSET %s %s %s"),
                                    key.c_str(), field.c_str(), value.c_str());

  if (status) {
    spdlog::info("[Redis]: Execute command [ HSET key = {0}, field = {1}, value = {2}] "
                 "successfully!",
                 key.c_str(), field.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::delValueFromHash(const std::string &key,
                                           const std::string &field) {

  if (key.empty()) {
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("HDEL %s %s"),
                                              key.c_str(), field.c_str());

  if (status) {
    spdlog::info("[Redis]: Execute command [ HDEL key = {0}, field = {1}] "
                 "successfully!",
                 key.c_str(), field.c_str());
    return true;
  }
  spdlog::error("[Redis]: The command did not execute successfully");
  return false;
}

bool redis::RedisContext::leftPush(const std::string &key,
                                   const std::string &value) {

  if (key.empty()) {
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("LPUSH %s %s"),
                                              key.c_str(), value.c_str());
  if (status) {
    spdlog::info(
        "[Redis]: Execute command  [ LPUSH key = {0}, value = {1}]  successfully!",
        key.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::rightPush(const std::string &key,
                                    const std::string &value) {

  if (key.empty()) {
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("RPUSH %s %s"),
                                              key.c_str(), value.c_str());
  if (status) {
    spdlog::info(
        "[Redis]: Execute command  [ RPUSH key = {0}, value = {1}]  successfully!",
        key.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::delPair(const std::string &key) {

  if (key.empty()) {
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status =
      m_replyDelegate->redisCommand(*this, std::string("DEL %s"), key.c_str());
  if (status) {
    spdlog::info("[Redis]: Execute command [ DEL key = {} ]successfully!", key.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::existKey(const std::string &key) {

  if (key.empty()) {
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("exists %s"),
                                              key.c_str());
  if (status) {
    spdlog::info("[Redis]:Execute command [ exists key = {}] successfully!",
                 key.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::heartBeat() {
          std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
          auto status = m_replyDelegate->redisCommand(*this, std::string("PING"));
          if (!m_replyDelegate->redisCommand(*this, std::string("PING"))) {
                    return false;
          }

          if (m_replyDelegate->getType().has_value() &&
                    m_replyDelegate->getType().value() != REDIS_REPLY_STRING) {
                    return false;
          }
          if (m_replyDelegate->getMessage() == "PONG") {
                    spdlog::info("[Redis]: Execute command [ PING ] successfully!");
                    return true;
          }
          return false;
}

std::optional<std::string>
redis::RedisContext::checkValue(const std::string &key) {

  if (key.empty()) {
    return std::nullopt;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("GET %s"),
                                     key.c_str())) {
    return std::nullopt;
  }
  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() != REDIS_REPLY_STRING) {
    return std::nullopt;
  }
  spdlog::info("[Redis]: Execute command [ GET key = %s ] successfully!", key.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::leftPop(const std::string &key) {

  if (key.empty()) {
    return std::nullopt;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("LPOP %s"),
                                     key.c_str())) {
    return std::nullopt;
  }
  if (!m_replyDelegate->getMessage().has_value()) {
    return std::nullopt;
  }
  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() == REDIS_REPLY_NIL) {
    return std::nullopt;
  }
  spdlog::info("[Redis]: Execute command [ LPOP key = {} ] successfully!", key.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::rightPop(const std::string &key) {

  if (key.empty()) {
    return std::nullopt;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("RPOP %s"),
                                     key.c_str())) {
    return std::nullopt;
  }
  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() == REDIS_REPLY_NIL) {
    return std::nullopt;
  }
  spdlog::info("[Redis]: Execute command  [ RPOP key = {}] successfully!", key.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::getValueFromHash(const std::string &key,
                                      const std::string &field) {

  if (key.empty()) {
    return std::nullopt;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("HGET %s %s"),
                                     key.c_str(), field.c_str())) {
    return std::nullopt;
  }

  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() == REDIS_REPLY_NIL) {
    return std::nullopt;
  }

  spdlog::info("Execute command [ HGET key = {0}, field = {1} ] successfully!",
               key.c_str(), field.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::acquire(const std::string &lockName,
                             const std::size_t waitTime, const std::size_t EXPX,
                             TimeUnit unit) {

  if (lockName.empty()) {
    return std::nullopt;
  }

  auto identifer = tools::userTokenGenerator();

  // Add additional lock name format
  std::string full_lock_name = std::string(lock) + lockName;

  // Calculate Redis Wait Time According to TimeUnit
  auto startTime = std::chrono::high_resolution_clock::now();
  auto endTime = std::chrono::high_resolution_clock::now();
  if (unit == TimeUnit::Seconds) {
    endTime = startTime + std::chrono::seconds(waitTime);
  } else if (unit == TimeUnit::Milliseconds) {
    endTime = startTime + std::chrono::milliseconds(waitTime);
  } else {
    spdlog::error("[REDIS]: Time Unit Error!");
    return std::nullopt;
  }

  while (std::chrono::high_resolution_clock::now() < endTime) {
    if (acquireLock(full_lock_name, identifer, EXPX, unit)) {
      return identifer;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return std::nullopt;
}

bool redis::RedisContext::acquireLock(const std::string &lockName,
                                      const std::string &identifer,
                                      const std::size_t EXPX, TimeUnit unit) {

  if (lockName.empty() || identifer.empty()) {
    return false;
  }

  std::string inputSchema;
  inputSchema.clear();
  if (unit == TimeUnit::Seconds) {
    inputSchema = std::string("SET %s %s NX EX %d");
  } else if (unit == TimeUnit::Milliseconds) {
    inputSchema = std::string("SET %s %s NX PX %d");
  } else {
    spdlog::error("[REDIS]: Time Unit Error!");
    return false;
  }

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();

  auto status = m_replyDelegate->redisCommand(
      *this, inputSchema, lockName.c_str(), identifer.c_str(), EXPX);

  if (status) {
    spdlog::info("Execute command [ SET key = {0}, value = {1}, timeout = {2}] "
                 "successfully!",
                 lockName.c_str(), identifer.c_str(), EXPX);
    return true;
  }
  return false;
}

bool redis::RedisContext::release(const std::string &lockName,
                                  const std::string &identifer) {
  if (lockName.empty() || identifer.empty()) {
    return false;
  }

  // Add additional lock name format
  std::string full_lock_name = std::string(lock) + lockName;
  return releaseLock(full_lock_name, identifer);
}

bool redis::RedisContext::releaseLock(const std::string &lockName,
                                      const std::string &identifer) {

  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();

  // Use EVAL to execute lua script
  // param 1: lua script itself
  // param 2: key
  // param 3: value
  auto status = m_replyDelegate->redisCommand(
      *this, "EVAL %s 1 %s %s", release_lock_lua_script, lockName.c_str(),
      identifer.c_str());

  if (status) {
    spdlog::info("Execute Lua Script [ EVAL {0} 1 {1} {2}] successfully!",
                 release_lock_lua_script, lockName.c_str(), identifer.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::checkError() {
  if (m_redisContext.get() == nullptr) {
    spdlog::error("Connection to Redis server failed! No instance!");
    return m_valid; // false;
  }

  /*error occured*/
  if (m_redisContext->err) {
    spdlog::error("Connection to Redis server failed! error code {}",
                  m_redisContext->errstr);
    return m_valid;
  }

  m_valid = true;
  return m_valid;
}

bool redis::RedisContext::checkAuth(std::string_view sv) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status =
      m_replyDelegate->redisCommand(*this, std::string("AUTH %s"), sv.data());
  if (status) {
    // spdlog::info("Excute command  [ AUTH ] successfully!");
  }
  return status;
}

std::optional<tools::RedisContextWrapper> redis::RedisContext::operator->() {
  if (isValid()) {
    return tools::RedisContextWrapper(m_redisContext.get());
  }
  return std::nullopt;
}
