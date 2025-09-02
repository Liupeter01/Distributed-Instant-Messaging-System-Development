#ifndef FILETRANSFERTHREAD_H
#define FILETRANSFERTHREAD_H

#include <MsgNode.hpp>
#include <QFile>
#include <QObject>
#include <QThread>
#include <singleton.hpp>

class FileTransferDialog;

class FileTransferThread : public QObject,
                           public Singleton<FileTransferThread> {
  Q_OBJECT
  friend class FileTransferDialog;
  friend class Singleton<FileTransferThread>;

  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;
  explicit FileTransferThread(QObject *parent = nullptr);

public:
  virtual ~FileTransferThread();

public:
  static std::size_t calculateBlockNumber(const std::size_t totalSize,
                                          const std::size_t chunkSize);

private:
  void registerSignal();
  void closeFile();

private slots:
  void slot_start_file_transmission(const QString &fileName,
                                    const QString &filePath,
                                    const std::size_t fileChunk);

  void slot_send_next_block();

signals:
  void signal_send_next_block();
  void signal_start_file_transmission(const QString &fileName,
                                      const QString &filePath,
                                      const std::size_t fileChunk);

private:
  QThread *m_thread;
  QFile m_file;

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
};

#endif // FILETRANSFERTHREAD_H
