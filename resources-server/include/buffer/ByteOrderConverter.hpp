#ifndef BYTEORDERCONVERTER_HPP
#define BYTEORDERCONVERTER_HPP
#include <type_traits>
#include <boost/asio/detail/socket_ops.hpp>

template <typename T, typename std::enable_if_t<sizeof(T) == sizeof(uint16_t), int> = 0>
T convert_from_network(T value) {
          return boost::asio::detail::socket_ops::network_to_host_short(value);
}

template <typename T, typename std::enable_if_t<sizeof(T) == sizeof(uint32_t), int> = 0>
T convert_from_network(T value) {
          return boost::asio::detail::socket_ops::network_to_host_long(value);
}

template <typename T, typename std::enable_if_t<sizeof(T) == sizeof(uint16_t), int> = 0>
T convert_to_network(T value) {
          return boost::asio::detail::socket_ops::host_to_network_short(value);
}

template <typename T, typename std::enable_if_t< sizeof(T) == sizeof(uint32_t), int> = 0>
T convert_to_network(T value) {
          return boost::asio::detail::socket_ops::host_to_network_long(value);
}

struct ByteOrderConverter {
          template <typename T>
          T operator()(T value) const {
                    return convert_from_network(value);
          }
};

struct ByteOrderConverterReverse {
          template <typename T>
          T operator()(T value) const {
                    return convert_to_network(value);
          }
};

#endif // BYTEORDERCONVERTER_HPP
