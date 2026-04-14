#pragma once
#ifndef LOGICEXECUTOR_H
#define LOGICEXECUTOR_H

#include <MsgNode.hpp>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <def.hpp>
#include <resourcestoragemanager.h>
#include <unordered_map>

class LogicMethod;

class LogicExecutor : public QObject {
  Q_OBJECT
  friend class LogicMethod;
  using Callbackfunction = std::function<void(const QJsonObject)>;
  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;

public:
  explicit LogicExecutor(QObject *parent = nullptr);
  virtual ~LogicExecutor() = default;

public:
  [[nodiscard]]
  static std::size_t calculateBlockNumber(const std::size_t totalSize,
                                          const std::size_t chunkSize);

signals:

  void signal_start_file_upload(const QString &fileName,
                                const QString &filePath,
                                const std::size_t fileChunk);

  // pause transmission
  void signal_pause_file_upload();

  // resume transmission
  void signal_resume_file_upload(const QString &fileName,
                                 const QString &filePath);

  void signal_send_next_block(const QString &checksum);

  // update all UI interfaces that relevant to avatar icons(qlabels)
  void signal_update_interfaces_avatar_icons(const QString &path);

private:
  void registerSignal();

private slots:

  void slot_send_next_block(const QString &checksum);

  void slot_start_file_upload(const QString &fileName, const QString &filePath,
                              const std::size_t fileChunk);

  // pause transmission
  void slot_pause_file_upload();

  // resume transmission
  void slot_resume_file_upload(const QString &fileName,
                               const QString &filePath);

  /*
   * slot_breakpoint_upload could serve for two main purposes
   * - indicate the process of file upload response, and start to prepare for
   * the next block!
   * - when user activate break point resume, it will start from the curr_size
   * of the file
   */

  void slot_breakpoint_upload(std::shared_ptr<FileTransferDesc> desc);

  /*
   * slot_breakpoint_download:
   * - The user should use it to write block_data to specific position in the
   * file
   * - this function will also update the downloading status in the
   * unordered_map
   */
  void slot_breakpoint_download(std::shared_ptr<FileTransferDesc> desc,
                                QByteArray decoded_data,
                                const std::size_t block_size);

private:
  std::size_t m_chunkSize = 4096;

  /*according to service type to execute callback*/
  std::unordered_map<ServiceType, Callbackfunction> m_callbacks;
};

#endif // LOGICEXECUTOR_H
