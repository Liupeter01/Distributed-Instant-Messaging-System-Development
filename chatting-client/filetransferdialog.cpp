#include "filetransferdialog.h"
#include "ui_filetransferdialog.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <def.hpp>
#include <resourcestoragemanager.h>
#include <logicmethod.h>
#include <tcpnetworkconnection.h>

FileTransferDialog::FileTransferDialog(std::shared_ptr<UserNameCard> id,
                                       const std::size_t fileChunk,
                                       QWidget *parent)
    : QDialog(parent), ui(new Ui::FileTransferDialog), m_filePath{},
      m_fileCheckSum{}, m_fileName{}, m_fileSize(0),
      m_fileChunk(fileChunk) /*init fileChunk size*/
      ,
      m_blockNumber(0) {
  ui->setupUi(this);

  /*server not connected*/
  ui->send_button->setDisabled(true);

  /*register network event*/
  registerNetworkEvent();

  registerSignals();

  /*set userinfo to current user for validation*/
  ResourceStorageManager::get_instance()->setUserInfo(id);
}

FileTransferDialog::~FileTransferDialog() {
  emit signal_terminate_resources_server();
  delete ui;
}

void FileTransferDialog::registerNetworkEvent() {

  connect(this, &FileTransferDialog::signal_connect2_resources_server,
          TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_connect2_resources_server);

  connect(this, &FileTransferDialog::signal_terminate_resources_server,
          TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_terminate_resources_server);
}

void FileTransferDialog::registerSignals()
{
    connect(LogicMethod::get_instance().get(), &LogicMethod::signal_data_transmission_status,
            this, [this](const QString &filename,
                         const std::size_t curr_seq,
                         const std::size_t curr_size,
                         const std::size_t total_size){

        ui->progressBar->setValue(curr_size);    //update progress bar

        /*transmission finished!*/
        if(curr_size >= ui->progressBar->maximum()){
              ui->send_button->setDisabled(false);
        }
    });
}

bool FileTransferDialog::validateFile(const QString &file) {
  QFileInfo info(file);

  if (!info.isFile() || !info.isReadable()) {
    return false;
  }

  m_filePath = file;
  m_fileSize = info.size();
  m_fileName = info.fileName();

  qDebug() << "File Name: " << m_fileName << " Has Been Loaded!\n"
           << "File Size = " << m_fileSize;

  return true;
}

void FileTransferDialog::initProgressBar(const std::size_t fileSize) {
  ui->progressBar->setRange(0, fileSize);
  ui->progressBar->setValue(0);
}

std::size_t
FileTransferDialog::calculateBlockNumber(const std::size_t totalSize,
                                         const std::size_t chunkSize) {
  return static_cast<size_t>(
      std::ceil(static_cast<double>(totalSize) / chunkSize));
}

void FileTransferDialog::on_open_file_button_clicked() {

  ui->send_button->setDisabled(true);
  const auto fileName =
      QFileDialog::getOpenFileName(nullptr, "Open File", "",
                                   "Text Files (*.txt);;Images (*.png "
                                   "*.jpg);;PDF Files (*.pdf);;All Files (*)");

  if (fileName.isEmpty()) {
    /*filename not loaded, do not think about upload*/
    return;
  }

  if (!validateFile(fileName)) {
    /*maybe its not a file and can not be read*/
    return;
  }

  /*more than 4GB*/
  if (m_fileSize > FOUR_GB) {
    return;
  }

  /*init progress bar*/
  initProgressBar(m_fileSize);

  /*update ui display*/
  ui->file_path->setText(m_filePath);
  ui->file_size_display->setText(QString::number(m_fileSize) + " byte");

  ui->send_button->setDisabled(false);
}

void FileTransferDialog::on_send_button_clicked() {

    //accumulate transferred size(from seq = 1 to n)
    std::size_t accumulate_transferred{0};

    //transfered size for a single seq(maybe seq =1, or seq = 2)
    std::size_t bytes_transferred_curr_sequence{0};

  ui->send_button->setDisabled(true);

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot User ReadOnly To Open File";
        return;
    }

    auto original_pos = file.pos();

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

  m_fileCheckSum = hash.result().toHex();

  /*there is how many blocks(loops) we need to parse the file*/
  m_blockNumber = calculateBlockNumber(m_fileSize, m_fileChunk);

  /*record current msg seq*/
  std::size_t cur_seq = 1;

  /*seek head of the file, before processing file!*/
  file.seek(original_pos);

  /*start to parse the file*/
  while (!file.atEnd()) {
    QJsonObject obj;

    /*it is not the final package(sometime, the final package size could not divided by m_fileChunk)*/
      bytes_transferred_curr_sequence =
        (cur_seq != m_blockNumber)
                               ? m_fileChunk
                               : m_fileSize - (cur_seq - 1) * m_fileChunk;

    if (!bytes_transferred_curr_sequence) {
          qDebug() << "transferred bytes = 0 in seq = " << cur_seq << "\n";
      break;
    }

    /*get a chunk size*/
    QByteArray buffer(file.read(bytes_transferred_curr_sequence));

    obj["filename"] = m_fileName;
    obj["checksum"] = QString(m_fileCheckSum);

    /*consist of pervious transmission size and this newest sequence size*/
    obj["cur_size"] =
        QString::number(accumulate_transferred + bytes_transferred_curr_sequence);
    obj["file_size"] = QString::number(m_fileSize);
    obj["block"] = QString(buffer.toBase64());
    obj["cur_seq"] = QString::number(cur_seq);
    obj["last_seq"] = QString::number(m_blockNumber);

    /*End of Transmission*/
    if (accumulate_transferred + bytes_transferred_curr_sequence >= m_fileSize) {
      obj["EOF"] = QString::number(1);
    }
    else{
        obj["EOF"] = QString::number(0);
    }

    QJsonDocument doc(obj);
    auto json_data = doc.toJson(QJsonDocument::Compact);

    std::shared_ptr<SendNodeType> send_buffer = std::make_shared<SendNodeType>(
        static_cast<uint16_t>(ServiceType::SERVICE_FILEUPLOADREQUEST),
        json_data, [](auto x) { return qToBigEndian(x); });

    TCPNetworkConnection::get_instance()->send_sequential_data_f(
        send_buffer, TargetServer::RESOURCESSERVER);

    /*update seq and accumulate size*/
    ++cur_seq;
    accumulate_transferred += bytes_transferred_curr_sequence;
  }

  file.close();

  ui->send_button->setDisabled(false);
}

void FileTransferDialog::on_connect_server_clicked() {
  auto ip = ui->server_addr->text();
  auto port = ui->server_port->text();

  if (!ip.isEmpty() || !port.isEmpty()) {
    qDebug() << "Invalid Ip and Port!";
    return;
  }

  ResourceStorageManager::get_instance()->set_host(ip);
  ResourceStorageManager::get_instance()->set_port(port);

  emit signal_connect2_resources_server();

  ui->connect_server->setDisabled(true);
  ui->send_button->setDisabled(false);
}
