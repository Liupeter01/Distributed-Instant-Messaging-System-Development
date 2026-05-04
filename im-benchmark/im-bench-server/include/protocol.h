#pragma once
#include <cstdint>
#include <cstring>
#pragma once
#include <def.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

constexpr uint16_t BENCH_ECHO_ID = static_cast<uint16_t>(ServiceType::SERVICE_BENCH_ECHO);  //  msg_id
constexpr size_t HEADER_SIZE = 4;           // 2B id + 2B length 

struct ParsedHeader {
          uint16_t id;
          uint16_t total_len;
          uint16_t body_len() const { return total_len - HEADER_SIZE; }
};

inline uint16_t to_be16(uint16_t v) {
          uint16_t r;
          uint8_t* p = reinterpret_cast<uint8_t*>(&r);
          p[0] = (v >> 8) & 0xFF;
          p[1] = v & 0xFF;
          return r;
}

inline uint16_t from_be16(uint16_t v) {
          const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
          return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

inline std::vector<uint8_t> make_packet(uint16_t id,
          const void* payload,
          size_t payload_len) {
          uint16_t total_len = static_cast<uint16_t>(HEADER_SIZE + payload_len);
          std::vector<uint8_t> buf(total_len);

          uint16_t be_id = to_be16(id);
          uint16_t be_len = to_be16(total_len);

          std::memcpy(&buf[0], &be_id, 2);
          std::memcpy(&buf[2], &be_len, 2);
          if (payload_len > 0) {
                    std::memcpy(&buf[4], payload, payload_len);
          }
          return buf;
}

inline ParsedHeader parse_header(const uint8_t* data) {
          uint16_t raw_id, raw_len;
          std::memcpy(&raw_id, data, 2);
          std::memcpy(&raw_len, data + 2, 2);
          return { from_be16(raw_id), from_be16(raw_len) };
}