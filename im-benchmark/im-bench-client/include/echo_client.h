#pragma once
#include <algorithm>
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <protocol.h>
#include <thread>
#include <vector>

using namespace boost::asio;
using namespace std::chrono;
using clk = steady_clock;

struct RttSample {
  uint64_t t_ms;
  double rtt_us;
};

struct Stats {
  std::atomic<uint64_t> sent{0};
  std::atomic<uint64_t> received{0};
  std::atomic<uint64_t> failed{0};
  std::atomic<uint64_t> connected{0};
  std::vector<RttSample> samples;
  std::mutex mu;
  clk::time_point start;
};

class BenchSession : public std::enable_shared_from_this<BenchSession> {
  ip::tcp::socket sock_;
  steady_timer timer_;
  Stats &stats_;
  std::array<uint8_t, 128> rbuf_;

public:
  BenchSession(io_context &ioc, Stats &s);
  void start(const ip::tcp::endpoint &ep);

private:
  void schedule_ping();

  void send_ping();

  void do_read();
};
