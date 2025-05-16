#include <redis/RedisReplyRAII.hpp>

bool redis::RedisReply::isSuccessful() const {
  if (m_redisReply.get() == nullptr) {
    return false;
  }

  // Check the type of reply and determine success
  switch (m_redisReply->type) {
  case REDIS_REPLY_INTEGER:
    // For commands like HSET, if the integer is >= 0, it's successful
    return m_redisReply->integer >= 0;

  case REDIS_REPLY_STATUS:
    // "OK" indicates success
    return std::string(m_redisReply->str) == "OK" ||
           std::string(m_redisReply->str) == "ok";

  case REDIS_REPLY_ARRAY:
    // Assuming success if the array contains elements (e.g., for LRANGE)
    return m_redisReply->elements > 0;

  case REDIS_REPLY_STRING:
    // For string replies, we assume success if the reply is not empty
    return m_redisReply->str != nullptr &&
           std::string(m_redisReply->str).length() > 0;

  case REDIS_REPLY_NIL:
    return false; // Nil replies indicate no data found (e.g., key doesn't
                  // exist)

  case REDIS_REPLY_ERROR:
  default:
    return false; // Any errors or unknown types are considered failures
  }
}

std::optional<long long> redis::RedisReply::getInterger() const {
  if (m_redisReply.get() != nullptr) {
    return m_redisReply->integer;
  }
  return std::nullopt;
}

std::optional<int> redis::RedisReply::getType() const {
  if (m_redisReply.get() != nullptr) {
    return m_redisReply->type;
  }
  return std::nullopt;
}

std::optional<std::string> redis::RedisReply::getMessage() const {
  if (m_redisReply.get() != nullptr) {
    return m_redisReply->str;
  }
  return std::nullopt;
}
