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

void LogicMethod::slot_resources_logic_handler(const uint16_t id, QJsonObject json)
{
    if(static_cast<ServiceType>(id) == ServiceType::SERVICE_FILEUPLOADRESPONSE ||
        static_cast<ServiceType>(id) == ServiceType::SERVICE_FILECHECKUPLOADPROGRESSRESPONSE){

        if (!json.contains("error")) {
            qDebug() << "Json Parse Error!";
            return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
            qDebug() << "File Upload Failed!!";
            return;
        }

        [[maybe_unused]] auto checksum = json["checksum"].toString();
        [[maybe_unused]] auto curr_seq = json["curr_seq"].toString().toUInt();
        [[maybe_unused]] auto curr_size = json["curr_size"].toString().toUInt();
        [[maybe_unused]] auto total_size = json["total_size"].toString().toUInt();
        [[maybe_unused]] auto eof = json["EOF"].toBool();

        emit signal_break_point_resume(checksum, curr_seq, curr_size, total_size, eof);

    }else{
        qDebug() << "Invalid Resources Service ID!";
    }
}

void LogicMethod::registerSignals() {

  /*incoming signal from FileTcpNetwork class*/
  connect(FileTCPNetwork::get_instance().get(),
          &FileTCPNetwork::signal_resources_logic_handler,
          this,
          &LogicMethod::slot_resources_logic_handler);

  connect(this, &LogicMethod::signal_break_point_resume, m_exec,
          &LogicExecutor::slot_break_point_resume);

  //forward signal to executor
  connect(this, &LogicMethod::signal_start_file_transmission, m_exec,
          &LogicExecutor::signal_start_file_transmission);

  connect(this, &LogicMethod::signal_pause_file_transmission, m_exec,
          &LogicExecutor::signal_pause_file_transmission);

  connect(this, &LogicMethod::signal_resume_file_transmission, m_exec,
          &LogicExecutor::signal_resume_file_transmission);

  connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
}

void LogicMethod::recordMD5Progress(const QString &md5,
                                    std::shared_ptr<QFileInfo> info) {

  std::lock_guard<std::mutex> _lckg(m_mtx);

  auto it = m_md5_cache.find(md5);

  // insert new element
  if (it == m_md5_cache.end()) {

    auto [_, status] = m_md5_cache.try_emplace(md5, info);
    if (!status)
      qDebug() << "md5 uuid exist!\n";

    return;
  }

  // Update Only
  if (it->second) {
    it->second.reset();
  }
  it->second = info;
}

std::optional<std::shared_ptr<QFileInfo>>
LogicMethod::getFileByMD5(const QString &md5) {

  std::lock_guard<std::mutex> _lckg(m_mtx);
  auto it = m_md5_cache.find(md5);
  if (it == m_md5_cache.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool LogicMethod::getPauseStatus() const { return m_pause; }
void LogicMethod::setPause(const bool status) { m_pause = status; }
