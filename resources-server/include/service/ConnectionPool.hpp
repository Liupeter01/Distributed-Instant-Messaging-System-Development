#pragma once
#ifndef _CONNECTIONPOOOL_HPP_
#define _CONNECTIONPOOOL_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <singleton/singleton.hpp>
#include <thread>
#include <tools/tools.hpp>

namespace connection {
/*please pass your new pool as template parameter*/
template <class WhichPool, typename _Type>
class ConnectionPool : public Singleton<WhichPool> {
  friend class Singleton<WhichPool>;

protected:
  ConnectionPool()
      : m_stop(false), m_queue_size(std::thread::hardware_concurrency() < 2
                                        ? 2
                                        : std::thread::hardware_concurrency()) {
  }

public:
  using stub = _Type;
  using stub_ptr = std::unique_ptr<_Type>;

  virtual ~ConnectionPool() { shutdown(); }

  void shutdown() {
    /*set stop flag to true*/
    m_stop = true;
    m_cv.notify_all();

    std::lock_guard<std::mutex> _lckg(m_mtx);
    while (!m_stub_queue.empty()) {
      m_stub_queue.pop();
    }
  }

  std::optional<stub_ptr> acquire() {
    std::unique_lock<std::mutex> _lckg(m_mtx);
    m_cv.wait(_lckg, [this]() { return !m_stub_queue.empty() || m_stop; });

    /*check m_stop flag*/
    if (m_stop) {
      return std::nullopt;
    }
    stub_ptr temp = std::move(m_stub_queue.front());
    m_stub_queue.pop();
    return temp;
  }

  void release(stub_ptr stub) {
    if (m_stop) {
      return;
    }
    std::lock_guard<std::mutex> _lckg(m_mtx);
    m_stub_queue.push(std::move(stub));
    m_cv.notify_one();
  }

protected:
  /*Stubpool stop flag*/
  std::atomic<bool> m_stop;

  /*Stub Ammount*/
  std::size_t m_queue_size;

  /*queue control*/
  std::mutex m_mtx;
  std::condition_variable m_cv;

  /*stub queue*/
  std::queue<stub_ptr> m_stub_queue;
};

/*
 * get stub automatically!
 */
template <typename WhichPool, typename _Type> class ConnectionRAII {
  using wrapper = tools::ResourcesWrapper<_Type>;

public:
  ConnectionRAII() : status(true) {
    auto optional = WhichPool::get_instance()->acquire();
    if (!optional.has_value()) {
      status = false;
    } else {
      m_stub = std::move(optional.value());
    }
  }
  virtual ~ConnectionRAII() {
    if (status) {
      WhichPool::get_instance()->release(std::move(m_stub));

      /*StubRAII failed!!*/
      status = false;
    }
  }
  std::optional<wrapper> operator->() {
    if (status) {
      return wrapper(m_stub.get());
    }
    return std::nullopt;
  }

private:
  std::atomic<bool> status; // load stub success flag
  std::unique_ptr<_Type> m_stub;
};
} // namespace connection

#endif
