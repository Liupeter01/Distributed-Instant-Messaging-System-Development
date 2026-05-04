#include <echo_client.h>

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "usage: " << argv[0] << " <ip> <port> <n_conns> <out_csv>\n";
    return 1;
  }

  auto ip_str = argv[1];
  uint16_t port = std::atoi(argv[2]);
  int N = std::atoi(argv[3]);
  std::string csv = argv[4];

  int nt = std::min(8u, std::thread::hardware_concurrency());

  std::vector<std::unique_ptr<io_context>> iocs;
  std::vector<std::unique_ptr<executor_work_guard<io_context::executor_type>>>
      guards;
  std::vector<std::thread> threads;

  for (int i = 0; i < nt; i++) {
    iocs.push_back(std::make_unique<io_context>());
    guards.push_back(
        std::make_unique<executor_work_guard<io_context::executor_type>>(
            make_work_guard(*iocs.back())));
  }

  Stats stats;
  stats.samples.reserve(N * 300);
  stats.start = clk::now();

  ip::tcp::endpoint ep(ip::make_address(ip_str), port);

  for (int i = 0; i < nt; i++) {
    threads.emplace_back([&iocs, i] { iocs[i]->run(); });
  }

  for (int i = 0; i < N; i++) {
    auto &ioc = *iocs[i % nt];
    std::make_shared<BenchSession>(ioc, stats)->start(ep);
  }

  // 5 minutes
  std::this_thread::sleep_for(std::chrono::seconds(300));

  for (auto &g : guards)
    g->reset();
  for (auto &ioc : iocs)
    ioc->stop();
  for (auto &t : threads)
    t.join();
  // Report
  std::cout << "N_conns:      " << N << "\n"
            << "connected:    " << stats.connected << "\n"
            << "sent:         " << stats.sent << "\n"
            << "received:     " << stats.received << "\n"
            << "failed:       " << stats.failed << "\n";

  if (stats.sent > 0)
    std::cout << "success_rate: " << (100.0 * stats.received / stats.sent)
              << "%\n";

  // CSV dump
  {
    std::ofstream f(csv);
    f << "t_ms,rtt_us\n";
    std::lock_guard lk(stats.mu);
    for (auto &s : stats.samples)
      f << s.t_ms << "," << s.rtt_us << "\n";
  }

  // Quick stats
  std::vector<double> rtts;
  rtts.reserve(stats.samples.size());
  for (auto &s : stats.samples)
    rtts.push_back(s.rtt_us);
  std::sort(rtts.begin(), rtts.end());
  if (!rtts.empty()) {
    auto pct = [&](int p) {
      return rtts[std::min<size_t>(rtts.size() * p / 100, rtts.size() - 1)];
    };
    std::cout << "rtt_p50: " << pct(50) << " us\n"
              << "rtt_p90: " << pct(90) << " us\n"
              << "rtt_p99: " << pct(99) << " us\n"
              << "rtt_max: " << rtts.back() << " us\n";
  }
  std::cout << "wrote " << stats.samples.size() << " samples to " << csv
            << "\n";
}
