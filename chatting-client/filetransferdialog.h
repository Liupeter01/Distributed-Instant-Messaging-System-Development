#ifndef FILETRANSFERDIALOG_H
#define FILETRANSFERDIALOG_H

#include <ByteOrderConverter.hpp>
#include <MsgNode.hpp>
#include <QByteArray>
#include <QDialog>
#include <memory>
#include <QThread>

#define GB_TO_BYTES(gb) ((gb) * 1024LL * 1024 * 1024)
#define FOUR_GB GB_TO_BYTES(4)

class UserNameCard;

namespace Ui {
class FileTransferDialog;
}

class FileTransferDialog : public QDialog {
  Q_OBJECT

  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;

public:
  explicit FileTransferDialog(std::shared_ptr<UserNameCard> id,
                              const std::size_t fileChunk = 2048,
                              QWidget *parent = nullptr);

  virtual ~FileTransferDialog();

protected:
  bool validateFile(const QString &file);
  void initProgressBar(const std::size_t fileSize);

private:
  void registerNetworkEvent();
  void registerSignals();

signals:
  /*return connection status to login class*/
  void signal_connection_status(bool status);

  void signal_connect2_resources_server();
  void signal_terminate_resources_server();
  void signal_start_file_transmission(const QString&fileName,
                                     const QString&filePath,
                                     const std::size_t fileChunk);

private slots:
  /*open file*/
  void on_open_file_button_clicked();

  /*upload to server*/
  void on_send_button_clicked();

  /*connect to server*/
  void on_connect_server_clicked();

  void slot_connection_status(bool status);

private:
  Ui::FileTransferDialog *ui;

  /*chunk size and chunk number consist of this file*/
  std::size_t m_fileChunk = 4096;
  std::size_t m_blockNumber = 0;

  /*file basic info*/
  QString m_filePath;
  QString m_fileName;
  std::size_t m_fileSize;
  std::size_t m_alreadySent;

  /*md5 checksum*/
  QByteArray m_fileCheckSum;
};

#endif // FILETRANSFERDIALOG_H
