#include "logicexecutor.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QDebug>
#include <QJsonDocument>
#include <filetcpnetwork.h>
#include <logicmethod.h>
#include <useraccountmanager.hpp>

[[nodiscard]]
std::size_t LogicExecutor::calculateBlockNumber(const std::size_t totalSize,
                                                const std::size_t chunkSize) {

  return static_cast<size_t>(
      std::ceil(static_cast<double>(totalSize) / chunkSize));
}

LogicExecutor::LogicExecutor(QObject *parent) : QObject(parent) {

  // register signal
  registerSignal();
}

void LogicExecutor::registerSignal() {

  connect(this, &LogicExecutor::signal_start_file_upload, this,
          &LogicExecutor::slot_start_file_upload);

    connect(this, &LogicExecutor::signal_send_first_block, this,
            &LogicExecutor::slot_send_first_block);

  connect(this, &LogicExecutor::signal_send_next_block, this,
          &LogicExecutor::slot_send_next_block);

  connect(this, &LogicExecutor::signal_pause_file_upload, this,
          &LogicExecutor::slot_pause_file_upload);

  connect(this, &LogicExecutor::signal_resume_file_upload, this,
          &LogicExecutor::slot_resume_file_upload);
}

void LogicExecutor::slot_send_first_block(const QString &checksum,
                                          std::shared_ptr<FileTransferDesc> desc)
{

    ResourceStorageManager::get_instance()->recordUnfinishedTask(
        /* QString */
        checksum,
        /* std::shared_ptr<FileTransferDesc> */
        desc);


    QFile file(desc->filePath);

    if (!file.isOpen()) {
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Cannot Use ReadOnly Mode To Open File";
            return;
        }
    }

    // this is the current file pointer startup point!
    file.seek(0);

    if (file.atEnd()) {
        qDebug() << "Rearch At the End of the File, Terminate Process!";
        return;
    }

    std::size_t bytes_transferred_curr_sequence =
        (1 != desc->last_sequence)
            ? m_chunkSize
            : desc->total_size;

    QByteArray buffer = file.read(bytes_transferred_curr_sequence);
    if (buffer.isEmpty()) {
        qDebug() << "transferred bytes = 0 in seq = 1\n";
        return;
    }

    QJsonObject obj;
    obj["uuid"] = UserAccountManager::get_instance()->get_uuid();
    obj["filename"] = desc->filename;
    obj["filepath"] = desc->filePath;

    obj["checksum"] = desc->checksum;

    obj["cur_seq"] = QString::number(1);
    obj["last_seq"] = QString::number(desc->last_sequence);

    obj["current_block_size"] = QString::number(bytes_transferred_curr_sequence);
    obj["transfered_size"] = QString::number(0);
    obj["total_size"] = QString::number(desc->total_size);

    obj["block"] = QString(buffer.toBase64());
    obj["EOF"] = QString::number((1 == desc->last_sequence) ? 1 : 0);

    FileTCPNetwork::get_instance()->send_buffer(
        ServiceType::SERVICE_FILEUPLOADREQUEST, std::move(obj));

    file.close();
}

void LogicExecutor::slot_send_next_block(const QString &checksum) {

  auto opt =
      ResourceStorageManager::get_instance()->getUnfinishedTasks(checksum);
  if (!opt.has_value()) {
    qDebug() << "Unexpected Error! File might already reached EOF or illegal "
                "request!\n";
    return;
  }

  auto transfer_data = opt.value();

  // open file
  QFile file(transfer_data->filePath);

  if (!file.isOpen()) {
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot Use ReadOnly Mode To Open File";
      return;
    }
  }

  // this is the current file pointer startup point!
  file.seek(transfer_data->transfered_size);

  // Did user click paused?
  if (LogicMethod::get_instance()->getPauseStatus()) {
    return;
  }

  if (file.atEnd()) {
    qDebug() << "Rearch At the End of the File, Terminate Process!";
    return;
  }

  std::size_t bytes_transferred_curr_sequence =
      (transfer_data->curr_sequence != transfer_data->last_sequence)
          ? m_chunkSize
          : transfer_data->total_size -
                (transfer_data->curr_sequence - 1) * m_chunkSize;

  QByteArray buffer = file.read(bytes_transferred_curr_sequence);
  if (buffer.isEmpty()) {
    qDebug() << "transferred bytes = 0 in seq = "
             << transfer_data->curr_sequence << "\n";
    return;
  }

  QJsonObject obj;
  obj["uuid"] = UserAccountManager::get_instance()->get_uuid();
  obj["filename"] = transfer_data->filename;
  obj["filepath"] = transfer_data->filePath;

  obj["checksum"] = transfer_data->checksum;

  obj["cur_seq"] = QString::number(transfer_data->curr_sequence);
  obj["last_seq"] = QString::number(transfer_data->last_sequence);

  obj["current_block_size"] = QString::number(bytes_transferred_curr_sequence);
  obj["transfered_size"] = QString::number(transfer_data->transfered_size);
  obj["total_size"] = QString::number(transfer_data->total_size);

  obj["block"] = QString(buffer.toBase64());
  obj["EOF"] = QString::number((transfer_data->curr_sequence ==transfer_data->last_sequence) ? 1 : 0);

  FileTCPNetwork::get_instance()->send_buffer(
      ServiceType::SERVICE_FILEUPLOADREQUEST, std::move(obj));

  file.close();
}

void LogicExecutor::slot_start_file_upload(const QString &fileName,
                                           const QString &filePath,
                                           const std::size_t fileChunk) {

  if (!fileChunk || filePath.isEmpty() || fileName.isEmpty()) {
    qDebug() << "Invalid File Status(Error size or Invalid path)!\n";
    return;
  }

  m_chunkSize = fileChunk;

  QFile file(filePath);
  if (!file.isOpen()) {
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot Use ReadOnly Mode To Open File";
      return;
    }
  }

  auto fileSize = file.size();
  auto totalBlocks = calculateBlockNumber(fileSize, fileChunk);

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

  QString checksum =
      QString::fromStdString(hash.result().toHex().toStdString());

  emit signal_send_first_block(checksum,   std::make_shared<FileTransferDesc>(fileName, checksum, filePath, 1,
                                                                            totalBlocks, false, 0, fileSize));

  file.close();
}

void LogicExecutor::slot_pause_file_upload() {
  LogicMethod::get_instance()->setPause(true);
}

void LogicExecutor::slot_resume_file_upload(const QString &fileName,
                                            const QString &filePath) {

  if (fileName.isEmpty() || filePath.isEmpty()) {
    qDebug() << "Invalid File Status(Invalid path)!\n";
    return;
  }

  QFile file(filePath);
  if (!file.isOpen()) {
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot Use ReadOnly Mode To Open File";
      return;
    }
  }

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

  QString checksum =
      QString::fromStdString(hash.result().toHex().toStdString());

  file.seek(0);
  file.close();

  QJsonObject obj;
  obj["filename"] = fileName;
  obj["checksum"] = checksum;

  FileTCPNetwork::get_instance()->send_buffer(
      ServiceType::SERVICE_FILECHECKUPLOADPROGRESSREQUEST, std::move(obj));
}

void LogicExecutor::slot_breakpoint_upload(
    std::shared_ptr<FileTransferDesc> desc) {
  ResourceStorageManager::get_instance()->removeUnfinishedTask(desc->checksum);

  // file transfer finished!
  if (desc->isEOF) {
    // reset pause status to prevent unexpected error!
    LogicMethod::get_instance()->setPause(false);

    return;
  }

  //if is not eof then increment seq
  ++desc->curr_sequence;

  ResourceStorageManager::get_instance()->recordUnfinishedTask(desc->checksum,
                                                               desc);

  emit signal_send_next_block(desc->checksum);
}

void LogicExecutor::slot_breakpoint_download(
    std::shared_ptr<FileTransferDesc> desc, QByteArray decoded_data,
    const std::size_t block_size) {

  if (!desc)
    return;

  auto exist = ResourceStorageManager::get_instance()->getUnfinishedTasks(
      desc->filename);
  if (!exist.has_value()) {
    qDebug() << desc->filename << " Not Exist in Unfinished task!\n";
    return;
  }

  QIODevice::OpenMode mode = QIODevice::WriteOnly;
  QFile file(desc->filePath);

  // NOT first package
  if (desc->curr_sequence != 1) {
    mode |= QIODevice::Append;
  }

  if (file.open(mode)) {
    qDebug() << "Unable to open file for writing!\n";
    return;
  }

  uint64_t bytesWritten = file.write(decoded_data);

  file.close();

  // update transfered size!
  desc->transfered_size += bytesWritten;

  qDebug() << bytesWritten
           << " size of data has been written to the file, expect: "
           << block_size << "\nCurrent Progress: " << desc->transfered_size
           << "/" << desc->total_size << "("
           << (desc->transfered_size * 100.f / desc->total_size) << "%)\n";

  ResourceStorageManager::get_instance()->removeUnfinishedTask(desc->filename);

  if (desc->isEOF) {
    qDebug() << "File: " << desc->filename
             << " Downloading Process Finished!\n";

    emit signal_update_interfaces_avatar_icons(desc->filePath);

    // emit
    return;
  }

  // update curr_sequence(DO NOT FORGOT ABOUT THIS!!!!)
  desc->curr_sequence += 1;

  ResourceStorageManager::get_instance()->recordUnfinishedTask(desc->filename,
                                                               desc);

  FileTCPNetwork::get_instance()->send_download_request(desc);
}
