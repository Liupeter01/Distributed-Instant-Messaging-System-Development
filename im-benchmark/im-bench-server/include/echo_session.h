#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <protocol.h>

using namespace boost::asio;
using ip::tcp;

class EchoSession : public std::enable_shared_from_this<EchoSession> {
          tcp::socket sock_;
          std::array<uint8_t, 4> hdr_buf_;
          std::vector<uint8_t> body_buf_;

public:
          explicit EchoSession(tcp::socket s);
          void start();

private:
          void read_header();
          void read_body(uint16_t id);
          void send_echo(uint16_t id, const void* payload, size_t len);
};