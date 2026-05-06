#pragma once
#ifndef _IOSERVICEPOOL_HPP_
#define _IOSERVICEPOOL_HPP_
#include <atomic>
#include <boost/asio.hpp>
#include <singleton/singleton.hpp>

class IOServicePool : public Singleton<IOServicePool> {
  friend class Singleton<IOServicePool>;
  using ioc = boost::asio::io_context;
  using work = boost::asio::io_context::work;
  using work_ptr = std::unique_ptr<work>;

public:
  ~IOServicePool() = default;

  void shutdown() {
    for (auto &work : m_work_pool) {
      boost::asio::io_context &ioc = work->get_io_context();
      if (!ioc.stopped()) {
        ioc.stop();
      }

      /*deallocate*/
      work.reset();
    }

    /*join all the threads*/
    for (auto &thread : m_thread_pool) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  boost::asio::io_context &getIOServiceContext() {
    boost::asio::io_context &result = m_ioc_pool.at(m_curr.fetch_add(1));
    if (m_curr.load() == this->m_ioc_pool.size()) {
      m_curr.store(0);
    }
    return result;
  }

private:
  IOServicePool()
      : IOServicePool(std::thread::hardware_concurrency() < 2
                          ? 2
                          : std::thread::hardware_concurrency()) {}

  IOServicePool(std::size_t threads)
      : m_curr(0), m_ioc_pool(threads), m_thread_pool(threads),
        m_work_pool(threads) {
    for (std::size_t i = 0; i < threads; ++i) {
      /*create ioc_context guarantee io_context won't quite automatically!*/
      m_work_pool.at(i) = std::make_unique<work>(m_ioc_pool.at(i));

      /*create thread*/
      m_thread_pool.emplace_back([this, i]() { m_ioc_pool.at(i).run(); });
    }
  }

private:
  std::vector<std::thread> m_thread_pool;

  /*
   * using RR method dispatch io_context,
   * points to current io_context
   */
  std::atomic<std::size_t> m_curr;

  /*io_context pool*/
  std::vector<ioc> m_ioc_pool;

  /*preventing io_context from exitting*/
  std::vector<work_ptr> m_work_pool;
};

#endif // !_IOSERVICEPOOL_HPP_
