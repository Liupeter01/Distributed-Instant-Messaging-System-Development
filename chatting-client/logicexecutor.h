#pragma once
#ifndef LOGICEXECUTOR_H
#define LOGICEXECUTOR_H

#include <MsgNode.hpp>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <def.hpp>
#include <unordered_map>

class LogicMethod;

class LogicExecutor : public QObject {
  Q_OBJECT
  friend class LogicMethod;
  using Callbackfunction = std::function<void(const QJsonObject)>;
  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;

public:
  explicit LogicExecutor(QObject *parent = nullptr);
  virtual ~LogicExecutor();

public:
  [[nodiscard]]
  static std::size_t calculateBlockNumber(const std::size_t totalSize,
                                          const std::size_t chunkSize);

signals:

  void signal_start_file_transmission(const QString &fileName,
                                      const QString &filePath,
                                      const std::size_t fileChunk);

  // pause transmission
  void signal_pause_file_transmission();

  // resume transmission
  void signal_resume_file_transmission();

  void signal_send_next_block(const QString &checksum);

private:
  void registerSignal();
  //void registerCallbacks();

private slots:

  void slot_send_next_block(const QString &checksum);

  void slot_start_file_transmission(const QString &fileName,
                                    const QString &filePath,
                                    const std::size_t fileChunk);

  // pause transmission
  void slot_pause_file_transmission();

  // resume transmission
  void slot_resume_file_transmission();

  /*
   * slot_break_point_resume could serve for two main purposes
   * - indicate the process of file upload response, and start to prepare for the next block!
   * - when user activate break point resume, it will start from the curr_size of the file
   */
  void slot_break_point_resume(QString checksum,
                                 const std::size_t curr_seq,
                                 const std::size_t curr_size,
                                 const std::size_t total_size,
                                 const bool eof);

private:
  QString m_fileName;
  QString m_filePath;
  QString m_fileCheckSum;

  std::size_t m_fileSize = 0;
  std::size_t m_fileChunk = 0;
  std::size_t m_curSeq = 1;
  std::size_t m_totalBlocks = 0;

  // accumulate transferred size(from seq = 1 to n)
  std::size_t accumulate_transferred{0};

  // transfered size for a single seq(maybe seq =1, or seq = 2)
  std::size_t bytes_transferred_curr_sequence{0};

  /*according to service type to execute callback*/
  std::unordered_map<ServiceType, Callbackfunction> m_callbacks;
};

#endif // LOGICEXECUTOR_H
