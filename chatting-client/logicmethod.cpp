#include "logicmethod.h"
#include <filetcpnetwork.h>

LogicMethod::LogicMethod(QObject *parent)
    : QObject{parent}, m_thread(new QThread()),
      m_exec(new LogicExecutor()) {

  //meta type
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
  /*incoming signal from resources server*/
  connect(this, &LogicMethod::signal_resources_logic_handler, m_exec,
          &LogicExecutor::slot_resources_logic_handler);

  /*
   * parse the data inside logicexecutor
   * retrieve data transmission status(propotion)
   */
  connect(m_exec, &LogicExecutor::signal_data_transmission_status, this,
          &LogicMethod::signal_data_transmission_status);

  connect(this, &LogicMethod::signal_start_file_transmission, m_exec,
          &LogicExecutor::signal_start_file_transmission);

  connect(this, &LogicMethod::signal_pause_file_transmission, m_exec,
          &LogicExecutor::signal_pause_file_transmission);

  connect(this, &LogicMethod::signal_resume_file_transmission, m_exec,
          &LogicExecutor::signal_resume_file_transmission);

  connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
}

void LogicMethod::recordMD5Progress(const QString &md5,
                                    std::shared_ptr<QFileInfo> info){

    std::lock_guard<std::mutex> _lckg(m_mtx);

    auto it = m_md5_cache.find(md5);

    //only update
    if(it != m_md5_cache.end()){
        if(it->second){
            it->second.reset();
        }
        it->second = info;
        return;
    }
    auto [_, status] = m_md5_cache.try_emplace(md5, info);
    if(!status){
        qDebug() << "md5 uuid exist!\n";
    }
}

std::optional<std::shared_ptr<QFileInfo>>
LogicMethod::getFileByMD5(const QString &md5){

    std::lock_guard<std::mutex> _lckg(m_mtx);
    auto it = m_md5_cache.find(md5);
    if(it == m_md5_cache.end()){
        return std::nullopt;
    }
    return it->second;
}

bool LogicMethod::getPauseStatus() const { return m_pause; }
void LogicMethod::setPause(const bool status) { m_pause = status; }
