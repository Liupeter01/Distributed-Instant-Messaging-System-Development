#include <echo_server.h>

int main(int argc, char** argv) {

          uint16_t port = argc > 1 ? std::atoi(argv[1]) : 8888;
          int nthreads = argc > 2 ? std::atoi(argv[2]) : 4;

          io_context ioc;
          EchoServer srv(ioc, port);

          std::vector<std::thread> threads;
          for (int i = 0; i < nthreads; i++) {
                    threads.emplace_back([&ioc] { ioc.run(); });
          }
          for (auto& t : threads) t.join();
}