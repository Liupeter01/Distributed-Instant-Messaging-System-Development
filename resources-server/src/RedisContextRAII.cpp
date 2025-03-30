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
    spdlog::info("Connection to Redis server success!");
    checkAuth(password);
  }
}

bool redis::RedisContext::isValid() { return m_valid; }

bool redis::RedisContext::setValue(const std::string &key,
                                   const std::string &value) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("SET %s %s"),
                                              key.c_str(), value.c_str());
  if (status) {
    spdlog::info("Excute command [ SET key = {0}, value = {1}] successfully!",
                 key.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::setValue2Hash(const std::string &key,
                                        const std::string &field,
                                        const std::string &value) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status =
      m_replyDelegate->redisCommand(*this, std::string("HSET %s %s %s"),
                                    key.c_str(), field.c_str(), value.c_str());

  if (status) {
    spdlog::info("Excute command [ HSET key = {0}, field = {1}, value = {2}] "
                 "successfully!",
                 key.c_str(), field.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::delValueFromHash(const std::string &key,
                                           const std::string &field) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("HDEL %s %s"),
                                              key.c_str(), field.c_str());

  if (status) {
    spdlog::info("Excute command [ HDEL key = {0}, field = {1}] "
                 "successfully!",
                 key.c_str(), field.c_str());
    return true;
  }
  spdlog::error("The command did not execute successfully");
  return false;
}

bool redis::RedisContext::leftPush(const std::string &key,
                                   const std::string &value) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("LPUSH %s %s"),
                                              key.c_str(), value.c_str());
  if (status) {
    spdlog::info(
        "Excute command  [ LPUSH key = {0}, value = {1}]  successfully!",
        key.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::rightPush(const std::string &key,
                                    const std::string &value) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("RPUSH %s %s"),
                                              key.c_str(), value.c_str());
  if (status) {
    spdlog::info(
        "Excute command  [ RPUSH key = {0}, value = {1}]  successfully!",
        key.c_str(), value.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::delPair(const std::string &key) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status =
      m_replyDelegate->redisCommand(*this, std::string("DEL %s"), key.c_str());
  if (status) {
    spdlog::info("Excute command [ DEL key = {} ]successfully!", key.c_str());
    return true;
  }
  return false;
}

bool redis::RedisContext::existKey(const std::string &key) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  auto status = m_replyDelegate->redisCommand(*this, std::string("exists %s"),
                                              key.c_str());
  if (status) {
    spdlog::info("Excute command [ exists key = {}] successfully!",
                 key.c_str());
    return true;
  }
  return false;
}

std::optional<std::string>
redis::RedisContext::checkValue(const std::string &key) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("GET %s"),
                                     key.c_str())) {
    return std::nullopt;
  }
  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() != REDIS_REPLY_STRING) {
    return std::nullopt;
  }
  spdlog::info("Excute command [ GET key = %s ] successfully!", key.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::leftPop(const std::string &key) {
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
  spdlog::info("Excute command [ LPOP key = {} ] successfully!", key.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::rightPop(const std::string &key) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("RPOP %s"),
                                     key.c_str())) {
    return std::nullopt;
  }
  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() == REDIS_REPLY_NIL) {
    return std::nullopt;
  }
  spdlog::info("Excute command  [ RPOP key = {}] successfully!", key.c_str());
  return m_replyDelegate->getMessage();
}

std::optional<std::string>
redis::RedisContext::getValueFromHash(const std::string &key,
                                      const std::string &field) {
  std::unique_ptr<RedisReply> m_replyDelegate = std::make_unique<RedisReply>();
  if (!m_replyDelegate->redisCommand(*this, std::string("HGET %s %s"),
                                     key.c_str(), field.c_str())) {
    return std::nullopt;
  }

  if (m_replyDelegate->getType().has_value() &&
      m_replyDelegate->getType().value() == REDIS_REPLY_NIL) {
    return std::nullopt;
  }

  spdlog::info("Excute command [ HGET key = {0}, field = {1} ] successfully!",
               key.c_str(), field.c_str());
  return m_replyDelegate->getMessage();
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
    spdlog::info("Excute command  [ AUTH ] successfully!");
  }
  return status;
}

std::optional<tools::RedisContextWrapper> redis::RedisContext::operator->() {
  if (isValid()) {
    return tools::RedisContextWrapper(m_redisContext.get());
  }
  return std::nullopt;
}
