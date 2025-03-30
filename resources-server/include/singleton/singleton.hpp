#ifndef SINGLETON_HPP
#define SINGLETON_HPP
#include <memory>
#include <mutex>

template <typename _Ty> struct SingletonDeletor {
  void operator()(_Ty *value) { delete value; }
};

template <typename _Ty> class Singleton {
  static std::shared_ptr<_Ty> m_instance;

  /*Once we delete move ctor then all copy ctor functions are deleted!*/
  Singleton(Singleton &&) = delete;

protected:
  Singleton() = default;

public:
  ~Singleton() {}
  static std::shared_ptr<_Ty> &get_instance() {
    static std::once_flag flag;
    std::call_once(flag, [&]() {
      m_instance = std::shared_ptr<_Ty>(new _Ty, SingletonDeletor<_Ty>());
    });
    return m_instance;
  }
};

template <typename _Ty>
std::shared_ptr<_Ty> Singleton<_Ty>::m_instance = nullptr;

#endif // SINGLETON_HPP
