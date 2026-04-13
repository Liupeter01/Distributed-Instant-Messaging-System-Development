#include "logicmethod.h"
#include <filetcpnetwork.h>

LogicMethod::LogicMethod(QObject *parent)
    : QObject{parent}, m_thread(new QThread()), m_exec(new LogicExecutor()) {

  // meta type
  registerMetaType();

  /*move executor to subthread*/
  m_exec->moveToThread(m_thread);

  /*register signal*/
  registerSignals();

  /*start subthread*/
  m_thread->start();
}

void LogicMethod::registerMetaType() {
  qRegisterMetaType<std::shared_ptr<QFileInfo>>("std::shared_ptr<QFileInfo>");
}

void LogicMethod::registerSignals() {

  /*incoming signal from FileTcpNetwork class*/
  connect(FileTCPNetwork::get_instance().get(),
          &FileTCPNetwork::signal_breakpoint_upload,
          m_exec,
          &LogicExecutor::slot_breakpoint_upload);

  //forward signal to executor
  connect(this, &LogicMethod::signal_start_file_upload, m_exec,
          &LogicExecutor::signal_start_file_upload);

  connect(this, &LogicMethod::signal_pause_file_upload, m_exec,
          &LogicExecutor::signal_pause_file_upload);

  connect(this, &LogicMethod::signal_resume_file_upload, m_exec,
          &LogicExecutor::signal_resume_file_upload);

  //update all UI interfaces that relevant to avatar icons(qlabels)
  connect(m_exec,&LogicExecutor::signal_update_interfaces_avatar_icons,
          this, &LogicMethod::signal_update_interfaces_avatar_icons);

  connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
}

bool LogicMethod::getPauseStatus() const { return m_pause; }
void LogicMethod::setPause(const bool status) { m_pause = status; }
