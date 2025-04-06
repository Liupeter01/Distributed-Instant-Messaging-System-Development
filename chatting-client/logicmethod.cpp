#include "logicmethod.h"
#include <tcpnetworkconnection.h>

LogicMethod::LogicMethod(QObject *parent)
    : QObject{parent}, m_thread(new QThread(this)),
      m_exec(new LogicExecutor()) {
  /*move executor to subthread*/
  m_exec->moveToThread(m_thread);

  /*register signal*/
  registerSignals();

  /*start subthread*/
  m_thread->start();
}

LogicMethod::~LogicMethod() {
  m_thread->quit();
  m_thread->wait();
  m_thread->deleteLater();
  m_exec->deleteLater();
}

void LogicMethod::registerSignals() {
  /*incoming signal from resources server*/
  connect(this, &LogicMethod::signal_resources_logic_handler,
          m_exec, &LogicExecutor::slot_resources_logic_handler);

  /*
   * parse the data inside logicexecutor
   * retrieve data transmission status(propotion)
   */
  connect(m_exec, &LogicExecutor::signal_data_transmission_status, this,
          &LogicMethod::signal_data_transmission_status);
}
