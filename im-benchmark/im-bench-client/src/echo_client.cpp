#include <echo_client.h>

BenchSession::BenchSession(io_context &ioc, Stats &s)
    : sock_(ioc), timer_(ioc), stats_(s) {}

void BenchSession::start(const ip::tcp::endpoint &ep) {
  auto self = shared_from_this();
  sock_.async_connect(ep, [this, self](auto ec) {
    if (ec) {
      stats_.failed++;
      return;
    }
    sock_.set_option(ip::tcp::no_delay(true));
    stats_.connected++;
    schedule_ping();
    do_read();
  });
}

void BenchSession::schedule_ping() {
  auto self = shared_from_this();
  timer_.expires_after(seconds(1));
  timer_.async_wait([this, self](auto ec) {
    if (ec)
      return;
    send_ping();
    schedule_ping();
  });
}

void BenchSession::send_ping() {
  uint64_t ts =
      duration_cast<microseconds>(clk::now().time_since_epoch()).count();

  auto pkt = make_packet(BENCH_ECHO_ID, &ts, sizeof(ts));
  auto self = shared_from_this();
  boost::asio::async_write(sock_, buffer(pkt), [this, self](auto ec, auto) {
    if (ec) {
      stats_.failed++;
      return;
    }
    stats_.sent++;
  });
}

void BenchSession::do_read() {
  auto self = shared_from_this();
  sock_.async_read_some(buffer(rbuf_), [this, self](auto ec, auto n) {
    if (ec) {
      stats_.failed++;
      return;
    }
    if (n >= HEADER_SIZE + sizeof(uint64_t)) {
      auto hdr = parse_header(rbuf_.data());
      if (hdr.id == BENCH_ECHO_ID && hdr.body_len() >= sizeof(uint64_t)) {
        uint64_t echo_ts;
        std::memcpy(&echo_ts, &rbuf_[HEADER_SIZE], sizeof(echo_ts));
        auto now_us =
            duration_cast<microseconds>(clk::now().time_since_epoch()).count();
        double rtt_us = now_us - echo_ts;
        uint64_t t_ms =
            duration_cast<milliseconds>(clk::now() - stats_.start).count();
        {
          std::lock_guard lk(stats_.mu);
          stats_.samples.push_back({t_ms, rtt_us});
        }
        stats_.received++;
      }
    }
    do_read();
  });
}
