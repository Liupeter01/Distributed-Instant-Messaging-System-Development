#pragma once
#ifndef _TOOLS_HPP_
#define _TOOLS_HPP_
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace tools {
template <typename _Ty>
std::optional<_Ty> string_to_value(std::string_view value) {
  _Ty _temp_res{};
  std::from_chars_result res =
      std::from_chars(value.data(), value.data() + value.size(), _temp_res);
  if (res.ec == std::errc() && res.ptr == value.data() + value.size())
    return _temp_res;
  return std::nullopt;
}

} // namespace tools

#endif // !_TOOLS_HPP_
