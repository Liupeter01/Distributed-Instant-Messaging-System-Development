#include <echo_session.h>

EchoSession::EchoSession(boost::asio::io_context &_ioc) : sock_(_ioc) {
  // sock_.set_option(tcp::no_delay(true));
}

void EchoSession::start() { read_header(); }

void EchoSession::read_header() {
  auto self = shared_from_this();
  async_read(sock_, buffer(hdr_buf_), [this, self](auto ec, auto) {
    if (ec)
      return;
    auto hdr = parse_header(hdr_buf_.data());

    if (hdr.body_len() == 0) {
      send_echo(hdr.id, nullptr, 0);
      return;
    }

    body_buf_.resize(hdr.body_len());
    read_body(hdr.id);
  });
}

void EchoSession::read_body(uint16_t id) {
  auto self = shared_from_this();
  async_read(sock_, buffer(body_buf_), [this, self, id](auto ec, auto) {
    if (ec)
      return;
    send_echo(id, body_buf_.data(), body_buf_.size());
  });
}

void EchoSession::send_echo(uint16_t id, const void *payload, size_t len) {
  auto pkt = make_packet(id, payload, len);
  auto self = shared_from_this();
  async_write(sock_, buffer(pkt), [this, self](auto ec, auto) {
    if (ec)
      return;
    read_header();
  });
}
