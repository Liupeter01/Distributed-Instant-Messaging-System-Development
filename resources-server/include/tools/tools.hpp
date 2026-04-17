#pragma once
#ifndef _TOOLS_HPP_
#define _TOOLS_HPP_
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <charconv>
#include <hiredis.h>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace tools {
static std::string userTokenGenerator() {
  boost::uuids::uuid uuid_gen = boost::uuids::random_generator()();
  return boost::uuids::to_string(uuid_gen);
}

template <typename _Ty> class ResourcesWrapper {
public:
  ResourcesWrapper(_Ty *ctx) : m_ctx(ctx, [](_Ty *ptr) { /*do nothing*/ }) {}

  _Ty *get() const { return m_ctx.get(); }
  _Ty *operator->() const { return get(); }

private:
  std::shared_ptr<_Ty> m_ctx;
};

class RedisContextWrapper {
public:
  RedisContextWrapper(redisContext *ctx)
      : m_ctx(ctx, [](redisContext *ptr) { /*do nothing*/ }) {}

  redisContext *get() const { return m_ctx.get(); }

  operator redisContext *() const { return get(); }

private:
  std::shared_ptr<redisContext> m_ctx;
};

template <typename _Ty> struct RedisRAIIDeletor {
  void operator()(_Ty *ptr) {
    if (ptr == nullptr) {
      return;
    }
    if constexpr (std::is_same_v<std::decay_t<_Ty>, redisContext>) {
      redisFree(ptr);
    } else if constexpr (std::is_same_v<std::decay_t<_Ty>, redisReply>) {
      freeReplyObject(ptr);
    } else {
      delete ptr;
    }
  }
};

template <typename _Ty>
using RedisSmartPtr = std::unique_ptr<_Ty, RedisRAIIDeletor<_Ty>>;

template <typename _Ty>
std::optional<_Ty> string_to_value(std::string_view value) {
  _Ty _temp_res{};
  std::from_chars_result res =
      std::from_chars(value.data(), value.data() + value.size(), _temp_res);
  if (res.ec == std::errc() && res.ptr == value.data() + value.size())
    return _temp_res;
  return std::nullopt;
}

template <typename Derived, typename Base>
std::unique_ptr<Derived> static_unique_ptr_cast(std::unique_ptr<Base> &&base) {
  return std::unique_ptr<Derived>(static_cast<Derived *>(base.release()));
}

static std::string getString(const boost::json::object &obj, const char *key) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    throw std::runtime_error(std::string("missing field: ") + key);
  }
  return boost::json::value_to<std::string>(it->value());
}

static std::int64_t getInt64(const boost::json::object &obj, const char *key) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    throw std::runtime_error(std::string("missing field: ") + key);
  }

  const auto &v = it->value();

  if (v.is_int64()) {
    return v.as_int64();
  }
  if (v.is_uint64()) {
    return static_cast<std::int64_t>(v.as_uint64());
  }
  if (v.is_string()) {
    return std::stoll(boost::json::value_to<std::string>(v));
  }

  throw std::runtime_error(std::string("invalid integer field: ") + key);
}

static bool getBool(const boost::json::object &obj, const char *key) {

  auto it = obj.find(key);
  if (it == obj.end()) {
    throw std::runtime_error(std::string("missing field: ") + key);
  }

  const auto &v = it->value();

  if (v.is_bool()) {
    return v.as_bool();
  }
  if (v.is_string()) {
    auto s = boost::json::value_to<std::string>(v);
    return s == "true" || s == "1";
  }

  throw std::runtime_error(std::string("invalid bool field: ") + key);
}

} // namespace tools

#endif // !_TOOLS_HPP_
