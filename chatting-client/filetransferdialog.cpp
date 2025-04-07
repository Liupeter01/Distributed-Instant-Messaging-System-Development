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
#include <logicmethod.h>
#include <resourcestoragemanager.h>
#include <tcpnetworkconnection.h>
#include <filetransferthread.h>

FileTransferDialog::FileTransferDialog(std::shared_ptr<UserNameCard> id,
                                       const std::size_t fileChunk,
                                       QWidget *parent)
    : QDialog(parent), ui(new Ui::FileTransferDialog), m_filePath{},
    m_fileCheckSum{}, m_fileName{}, m_fileSize(0),m_alreadySent{0}
      ,m_fileChunk(fileChunk) /*init fileChunk size*/
    ,m_blockNumber(0) {

  ui->setupUi(this);

  /*server not connected*/
  ui->send_button->setDisabled(true);

  /*register network event*/
  registerNetworkEvent();

  registerSignals();

  /*set userinfo to current user for validation*/
  ResourceStorageManager::get_instance()->setUserInfo(id);

  ui->send_button->setDisabled(true);
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

  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_connection_status, this,
          &FileTransferDialog::signal_connection_status);
}

void FileTransferDialog::registerSignals() {

    /* update progress bar*/
    connect(LogicMethod::get_instance().get(),
            &LogicMethod::signal_data_transmission_status, this,
            [this](const QString &filename,
                   const std::size_t curr_seq,
                   const std::size_t curr_size,
                   const std::size_t total_size,
                   const bool eof) {

                ui->progressBar->setValue(curr_size);
                ui->progressBar->setMaximum(total_size);

                if(!eof){
                    ui->send_button->setDisabled(false);
                }
            });

  connect(this, &FileTransferDialog::signal_connection_status, this,
          &FileTransferDialog::slot_connection_status);

  connect(this, &FileTransferDialog::signal_start_file_transmission,
          FileTransferThread::get_instance().get(),
          &FileTransferThread::signal_start_file_transmission);
}

bool FileTransferDialog::validateFile(const QString &file) {
  QFileInfo info(file);

  if (!info.isFile() || !info.isReadable()) {
      ui->send_button->setDisabled(true);
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

void FileTransferDialog::on_open_file_button_clicked() {

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

 //m_uploadThread->moveToThread()
    ui->send_button->setDisabled(true);

 emit signal_start_file_transmission(m_fileName, m_filePath, m_fileChunk);
}

void FileTransferDialog::on_connect_server_clicked() {
    auto ip = ui->server_addr->text();
    auto port = ui->server_port->text();

    if (ip.isEmpty() || port.isEmpty()) {
        qDebug() << "Invalid Ip and Port!";
        return;
    }

    ResourceStorageManager::get_instance()->set_host(ip);
    ResourceStorageManager::get_instance()->set_port(port);

    emit signal_connect2_resources_server();

    ui->connect_server->setDisabled(true);
    ui->send_button->setDisabled(false);
}

void FileTransferDialog::slot_connection_status(bool status) {
  if (status) {
    qDebug() << "Resources Server Connected!\n";
  }
}
