#pragma once
#ifndef LOGICEXECUTOR_H
#define LOGICEXECUTOR_H

#include <QJsonObject>
#include <QObject>
#include <def.hpp>
#include <unordered_map>
#include <QFile>
#include <MsgNode.hpp>

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
  static
  std::size_t calculateBlockNumber(const std::size_t totalSize,
                                             const std::size_t chunkSize);

signals:

    void signal_start_file_transmission(const QString &fileName,
                                        const QString &filePath,
                                        const std::size_t fileChunk);

  //pause transmission
  void signal_pause_file_transmission();

  //resume transmission
  void signal_resume_file_transmission();

  void signal_send_next_block(const QString& checksum);

  /*data transmission status*/
  void signal_data_transmission_status(const QString &checksum,
                                       const std::size_t curr_seq,
                                       const std::size_t curr_size,
                                       const std::size_t total_size,
                                       const bool eof);

  private:
      void registerSignal();
      void registerCallbacks();

private slots:
  /*forward resources server's message to a standlone logic thread*/
  void slot_resources_logic_handler(const uint16_t id, const QJsonObject obj);

    void slot_send_next_block(const QString& checksum);
    void slot_start_file_transmission(const QString &fileName,
                                      const QString &filePath,
                                      const std::size_t fileChunk);


  //pause transmission
    void slot_pause_file_transmission();

    //resume transmission
    void slot_resume_file_transmission();

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
