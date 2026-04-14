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
#include <filetcpnetwork.h>
#include <logicmethod.h>
#include <resourcestoragemanager.h>

FileTransferDialog::FileTransferDialog(std::shared_ptr<UserNameCard> id,
                                       const std::size_t fileChunk,
                                       QWidget *parent)

    : QDialog(parent), ui(new Ui::FileTransferDialog), m_filePath{},
      m_state(TransferState::NOT_READY), m_fileCheckSum{}, m_fileName{},
      m_fileSize(0), m_alreadySent{0},
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

  ui->open_file_button->setDisabled(false);
  ui->send_button->setDisabled(true);

  ui->pauseandresume->setDisabled(true);
  ui->pauseandresume->setText(QString("pause"));

  m_state = TransferState::NOT_READY;
}

FileTransferDialog::~FileTransferDialog() {
  emit signal_terminate_resources_server();
  delete ui;
}

void FileTransferDialog::registerNetworkEvent() {

  connect(this, &FileTransferDialog::signal_connect2_resources_server,
          FileTCPNetwork::get_instance().get(),
          &FileTCPNetwork::signal_connect2_server);

  connect(this, &FileTransferDialog::signal_terminate_resources_server,
          FileTCPNetwork::get_instance().get(),
          &FileTCPNetwork::signal_terminate_server);

  connect(FileTCPNetwork::get_instance().get(),
          &FileTCPNetwork::signal_connection_status, this,
          &FileTransferDialog::signal_connection_status);
}

void FileTransferDialog::registerSignals() {

  /* update progress bar*/
  // connect(LogicMethod::get_instance().get(),
  //         &LogicMethod::signal_pause_file_upload, this,
  //         [this](const QString &checksum, const std::size_t curr_seq,
  //                const std::size_t curr_size, const std::size_t total_size,
  //                const bool eof) {
  //           if (eof) {
  //             ui->send_button->setDisabled(false);
  //             ui->open_file_button->setDisabled(false);
  //             ui->send_button->setDisabled(true);
  //             ui->pauseandresume->setDisabled(true);
  //             ui->pauseandresume->setText(QString("pause"));
  //             ui->progressBar->setValue(0);

  //             m_state = TransferState::END_TRANSMISSION;
  //             return;
  //           }

  //           ui->progressBar->setValue(curr_size);
  //           ui->progressBar->setMaximum(total_size);
  //         });

  connect(this, &FileTransferDialog::signal_connection_status, this,
          &FileTransferDialog::slot_connection_status);

  connect(this, &FileTransferDialog::signal_start_file_upload,
          LogicMethod::get_instance().get(),
          &LogicMethod::signal_start_file_upload);

  connect(this, &FileTransferDialog::signal_pause_file_upload,
          LogicMethod::get_instance().get(),
          &LogicMethod::signal_pause_file_upload);

  connect(this, &FileTransferDialog::signal_resume_file_upload,
          LogicMethod::get_instance().get(),
          &LogicMethod::signal_resume_file_upload);
}

bool FileTransferDialog::validateFile(const QString &file) {
  QFileInfo info(file);

  if (!info.isFile() || !info.isReadable()) {
    ui->open_file_button->setDisabled(false);
    ui->send_button->setDisabled(true);
    ui->pauseandresume->setDisabled(true);
    ui->pauseandresume->setText(QString("pause"));
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
  ui->progressBar->setMaximum(fileSize);
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

  ui->open_file_button->setDisabled(false);
  ui->send_button->setDisabled(false);
  ui->pauseandresume->setDisabled(true);
  ui->pauseandresume->setText(QString("pause"));

  m_state = TransferState::FILE_OPENED;
}

void FileTransferDialog::on_send_button_clicked() {

  ui->send_button->setDisabled(true);
  ui->open_file_button->setDisabled(true);
  ui->pauseandresume->setDisabled(false);
  ui->pauseandresume->setText(QString("pause"));

  m_state = TransferState::START_TRANSMISSION;

  emit signal_start_file_upload(m_fileName, m_filePath, m_fileChunk);
}

// void FileTransferDialog::on_connect_server_clicked() {
//   auto ip = ui->server_addr->text();
//   auto port = ui->server_port->text();

//   if (ip.isEmpty() || port.isEmpty()) {
//     qDebug() << "Invalid Ip and Port!";
//     return;
//   }

//   ResourceStorageManager::get_instance()->set_host(ip);
//   ResourceStorageManager::get_instance()->set_port(port);

//   emit signal_connect2_resources_server();

//   ui->connect_server->setDisabled(true);
//   ui->send_button->setDisabled(false);
// }

void FileTransferDialog::slot_connection_status(bool status) {
  if (status) {
    qDebug() << "Resources Server Connected!\n";
  }
}

void FileTransferDialog::pause_clicked() {
  ui->send_button->setDisabled(true);
  ui->open_file_button->setDisabled(false);

  ui->pauseandresume->setDisabled(false);
  ui->pauseandresume->setText(QString("resume"));

  m_state = going_to_pause;
  emit signal_pause_file_upload();
}

void FileTransferDialog::resume_clicked() {
  ui->send_button->setDisabled(true);
  ui->open_file_button->setDisabled(true);

  ui->pauseandresume->setDisabled(false);
  ui->pauseandresume->setText(QString("pause"));

  m_state = going_to_resume;

  emit signal_resume_file_upload(m_fileName, m_filePath);
}

void FileTransferDialog::on_pauseandresume_clicked() {

  if (static_cast<int>(m_state) & static_cast<int>(TransferState::NOT_READY) ||
      static_cast<int>(m_state) &
          static_cast<int>(TransferState::FILE_OPENED) ||
      static_cast<int>(m_state) &
          static_cast<int>(TransferState::END_TRANSMISSION)) {

    return;
  }

  // Currently, btn is in transmission instead of pause!
  if (static_cast<int>(m_state) &
          static_cast<int>(TransferState::START_TRANSMISSION) &&
      !(static_cast<int>(m_state) &
        static_cast<int>(TransferState::PAUSE_TRANSMISSION))) {

    pause_clicked();
    return;
  }

  // btn is pause!
  if (!(static_cast<int>(m_state) &
        static_cast<int>(TransferState::START_TRANSMISSION)) &&
      (static_cast<int>(m_state) &
       static_cast<int>(TransferState::PAUSE_TRANSMISSION))) {

    resume_clicked();
    return;
  }
}
