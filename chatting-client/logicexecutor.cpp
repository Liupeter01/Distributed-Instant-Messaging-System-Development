#include "logicexecutor.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QDebug>
#include <QJsonDocument>
#include <filetcpnetwork.h>
#include <logicmethod.h>

[[nodiscard]]
std::size_t LogicExecutor::calculateBlockNumber(const std::size_t totalSize,
                                                const std::size_t chunkSize) {

  return static_cast<size_t>(
      std::ceil(static_cast<double>(totalSize) / chunkSize));
}

LogicExecutor::LogicExecutor(QObject *parent) : QObject(parent) {

  // register callback
  registerCallbacks();

  // register signal
  registerSignal();
}

LogicExecutor::~LogicExecutor() {}

void LogicExecutor::registerSignal() {
  connect(this, &LogicExecutor::signal_start_file_transmission, this,
          &LogicExecutor::slot_start_file_transmission);

  connect(this, &LogicExecutor::signal_send_next_block, this,
          &LogicExecutor::slot_send_next_block);

  connect(this, &LogicExecutor::signal_pause_file_transmission, this,
          &LogicExecutor::slot_pause_file_transmission);

  connect(this, &LogicExecutor::signal_resume_file_transmission, this,
          &LogicExecutor::slot_resume_file_transmission);

  /*
   * flow control Server will return transmission status back(EOF or not?)
   * If NOT eof, this transmission progress could be paused!
   */
  connect(this, &LogicExecutor::signal_data_transmission_status, this,
          [this](const QString &checksum, const std::size_t curr_seq,
                 const std::size_t curr_size, const std::size_t total_size,
                 const bool eof) {
            if (eof) {
              m_curSeq = 0;
              accumulate_transferred = 0;

              // reset pause status to prevent unexpected error!
              LogicMethod::get_instance()->setPause(false);
            } else {
              m_curSeq = curr_seq + 1;
              accumulate_transferred = curr_size;
              emit signal_send_next_block(checksum);
            }
          });
}

void LogicExecutor::registerCallbacks() {

  auto updateNextBlocksInfo = [this](const QJsonObject json) {
    /*error occured!*/
    if (!json.contains("error")) {
      qDebug() << "Json Parse Error!";
      return;
    }
    if (json["error"].toInt() !=
        static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
      qDebug() << "File Transmission Resume Failed!";
      return;
    }

    [[maybe_unused]] auto checksum = json["checksum"].toString();
    [[maybe_unused]] auto curr_seq = json["curr_seq"].toString();
    [[maybe_unused]] auto curr_size = json["curr_size"].toString();
    [[maybe_unused]] auto total_size = json["total_size"].toString();
    [[maybe_unused]] auto eof = json["EOF"].toBool();

    // /*notifying the main UI interface to update progress bar!*/
    emit signal_data_transmission_status(checksum, curr_seq.toUInt(),
                                         curr_size.toUInt(),
                                         total_size.toUInt(), eof);
  };

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FILEUPLOADRESPONSE, updateNextBlocksInfo));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FILECHECKUPLOADPROGRESSRESPONSE,
      updateNextBlocksInfo));
}

void LogicExecutor::slot_resources_logic_handler(const uint16_t id,
                                                 const QJsonObject obj) {
  try {
    m_callbacks[static_cast<ServiceType>(id)](obj);
  } catch (const std::exception &e) {
    qDebug() << e.what();
  }
}

void LogicExecutor::slot_send_next_block(const QString &checksum) {

  // if(m_fileCheckSum != checksum){
  //     //TODO
  // }
  auto temp = LogicMethod::get_instance()->getFileByMD5(checksum);
  if (!temp.has_value()) {
    qDebug() << "File Not Found! Please Open a new one!\n";
    return;
  }

  // update checksum info
  m_fileCheckSum = checksum;

  // open file
  QFile file(temp.value()->filePath());

  if (!file.isOpen()) {
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot Use ReadOnly Mode To Open File";
      return;
    }
  }

  // this is the current file pointer startup point!
  file.seek(accumulate_transferred);

  // Did user click paused?
  if (LogicMethod::get_instance()->getPauseStatus()) {
    return;
  }

  if (file.atEnd()) {
    qDebug() << "Rearch At the End of the File, Terminate Process!";
    return;
  }

  std::size_t bytes_transferred_curr_sequence =
      (m_curSeq != m_totalBlocks) ? m_fileChunk
                                  : m_fileSize - (m_curSeq - 1) * m_fileChunk;

  QByteArray buffer = file.read(bytes_transferred_curr_sequence);
  if (buffer.isEmpty()) {
    qDebug() << "transferred bytes = 0 in seq = " << m_curSeq;
    return;
  }

  QJsonObject obj;
  obj["filename"] = m_fileName;
  obj["checksum"] = m_fileCheckSum;
  obj["cur_seq"] = QString::number(m_curSeq);
  obj["last_seq"] = QString::number(m_totalBlocks);
  obj["cur_size"] =
      QString::number(accumulate_transferred + bytes_transferred_curr_sequence);
  obj["file_size"] = QString::number(m_fileSize);
  obj["block"] = QString(buffer.toBase64());
  obj["EOF"] = (m_curSeq == m_totalBlocks) ? "1" : "0";

  FileTCPNetwork::get_instance()->send_buffer(
      ServiceType::SERVICE_FILEUPLOADREQUEST, std::move(obj));

  file.close();
}

void LogicExecutor::slot_start_file_transmission(const QString &fileName,
                                                 const QString &filePath,
                                                 const std::size_t fileChunk) {
  m_fileName = fileName;
  m_filePath = filePath;
  m_fileChunk = fileChunk;
  m_curSeq = 1;

  QFile file(filePath);
  if (!file.isOpen()) {
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot Use ReadOnly Mode To Open File";
      return;
    }
  }

  m_fileSize = file.size();
  m_totalBlocks = calculateBlockNumber(m_fileSize, m_fileChunk);

  /*use md5 to mark this file*/
  QCryptographicHash hash(QCryptographicHash::Md5);

  /*
   * try to hash the whole file and generate a unique record
   * WARNING: after call addData, the file pointer will move to
   * another position rather than current pos
   */
  if (!hash.addData(&file)) {
    qDebug() << "Hashing File Failed!";
    return;
  }

  m_fileCheckSum = QString::fromStdString(hash.result().toHex().toStdString());
  file.seek(0);

  std::shared_ptr<QFileInfo> info = std::make_shared<QFileInfo>(file);
  LogicMethod::get_instance()->recordMD5Progress(
      /* QString */ m_fileCheckSum,
      /* std::shared_ptr<QFileInfo> */ info);

  emit signal_send_next_block(m_fileCheckSum);

  file.close();
}

void LogicExecutor::slot_pause_file_transmission() {
  LogicMethod::get_instance()->setPause(true);
}

void LogicExecutor::slot_resume_file_transmission() {
  LogicMethod::get_instance()->setPause(false);

  QJsonObject obj;
  obj["checksum"] = m_fileCheckSum;

  FileTCPNetwork::get_instance()->send_buffer(
      ServiceType::SERVICE_FILECHECKUPLOADPROGRESSREQUEST, std::move(obj));
}
