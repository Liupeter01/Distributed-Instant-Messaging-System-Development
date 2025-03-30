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
  ~IOServicePool();
  void shutdown();
  boost::asio::io_context &getIOServiceContext();

private:
  IOServicePool();
  IOServicePool(std::size_t threads);

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
