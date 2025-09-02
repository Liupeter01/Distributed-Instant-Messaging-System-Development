#ifndef TCPNETWORKTHREAD_H
#define TCPNETWORKTHREAD_H
#include <QObject>
#include <QThread>
#include <memory>
#include <tcpnetworkconnection.h>

class TCPNetworkThread : public QObject,
                         public std::enable_shared_from_this<TCPNetworkThread> {
  Q_OBJECT

public:
  TCPNetworkThread(QObject *parent = nullptr)
      : QObject{parent},
        m_thread(new QThread()) { // you should NOT use parent at here!

    // now all tcpnetwork class are moving to this thread!
    TCPNetworkConnection::get_instance()->moveToThread(m_thread);

    registerSignal();
    m_thread->start();
  }

  virtual ~TCPNetworkThread() { m_thread->quit(); }

private:
  void registerSignal() {
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
  }

private:
  QThread *m_thread;
};

#endif // TCPNETWORKTHREAD_H
