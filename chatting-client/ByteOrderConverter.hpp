#ifndef BYTEORDERCONVERTER_HPP
#define BYTEORDERCONVERTER_HPP
#include <QtEndian>
#include <type_traits>

// SFINAE test for qFromBigEndian
template <typename T, typename = void>
struct has_qt_endian : std::false_type {};

template <typename T>
struct has_qt_endian<
    T, std::void_t<decltype(qToBigEndian<T>(std::declval<T>())),
                   decltype(qFromBigEndian<T>(std::declval<T>()))>>
    : std::true_type {};

// --- Qt Specialization ---
template <typename T,
          typename std::enable_if_t<has_qt_endian<T>::value, int> = 0>
T convert_from_network(T value) {
  return qFromBigEndian<T>(value);
}

template <typename T,
          typename std::enable_if_t<has_qt_endian<T>::value, int> = 0>
T convert_to_network(T value) {
  return qToBigEndian<T>(value);
}

struct ByteOrderConverter {
  template <typename T> T operator()(T value) const {
    return convert_from_network(value);
  }
};

struct ByteOrderConverterReverse {
  template <typename T> T operator()(T value) const {
    return convert_to_network(value);
  }
};

#endif // BYTEORDERCONVERTER_HPP
