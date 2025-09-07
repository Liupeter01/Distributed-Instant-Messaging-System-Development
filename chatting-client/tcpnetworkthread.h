#ifndef TCPNETWORKTHREAD_H
#define TCPNETWORKTHREAD_H
#include <QObject>
#include <QThread>
#include <chattingtcpnetwork.h>
#include <filetcpnetwork.h>
#include <memory>

class TCPNetworkThread : public QObject {
  Q_OBJECT

public:
  TCPNetworkThread(QObject *parent = nullptr)
      : QObject{parent},
        chatting_thread(new QThread()) // you should NOT use parent at here!
        ,
        resource_thread(new QThread()) // you should NOT use parent at here!
  {

    // now all tcpnetwork class are moving to this thread!
    ChattingTCPNetwork::get_instance()->moveToThread(chatting_thread);

    // now all tcpnetwork class are moving to this thread!
    FileTCPNetwork::get_instance()->moveToThread(resource_thread);

    registerSignal();

    chatting_thread->start();
    resource_thread->start();
  }

  virtual ~TCPNetworkThread() {
    chatting_thread->quit();
    resource_thread->quit();
  }

private:
  void registerSignal() {
    connect(chatting_thread, &QThread::finished, chatting_thread,
            &QObject::deleteLater);
    connect(resource_thread, &QThread::finished, resource_thread,
            &QObject::deleteLater);
  }

private:
  QThread *chatting_thread;
  QThread *resource_thread;
};

#endif // TCPNETWORKTHREAD_H
