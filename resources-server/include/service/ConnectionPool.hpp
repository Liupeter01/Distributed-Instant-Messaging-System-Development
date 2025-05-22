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

template <typename WhichPool, typename _Type> struct ConnectionRAII {
  using wrapper = tools::ResourcesWrapper<_Type>;
  ConnectionRAII(const ConnectionRAII&) = delete;
  ConnectionRAII& operator=(const ConnectionRAII&) = delete;

  ConnectionRAII(ConnectionRAII&&) = default;
  ConnectionRAII& operator=(ConnectionRAII&&) = default;

  ConnectionRAII() : status(true) {
            acquire();
  }
  virtual ~ConnectionRAII() {
            release();
  }
  std::optional<wrapper> operator->() {
    if (is_active()) {
      return wrapper(m_stub.get());
    }
    return std::nullopt;
  }

  std::optional <  std::reference_wrapper< std::unique_ptr<_Type>>> get_native() {
            if (is_active()) {
                      return  std::ref(m_stub);
            }
            return std::nullopt;
  }

  //Raii no longer needs to put this resources back to container!
  void invalidate() {
            status = false;
  }

  bool is_active() const {
            return status;
  }

protected:
  void acquire() {
            //valid resources retrieved!
            if (auto optional = WhichPool::get_instance()->acquire(); optional) {
                      m_stub = std::move(optional.value());
                      return;
            }

            invalidate();
  }

  void release() {
            if (is_active()) {

                      WhichPool::get_instance()->release(std::move(m_stub));

                      /*Stub no longer active*/
                      invalidate();
            }
  }

private:
  bool status; // load stub success flag
  std::unique_ptr<_Type> m_stub;
};
} // namespace connection

#endif
