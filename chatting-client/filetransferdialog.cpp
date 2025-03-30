#include <QFile>
#include <QDebug>
#include <def.hpp>
#include <QFileInfo>
#include <QByteArray>
#include <MsgNode.hpp>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCryptographicHash>
#include "filetransferdialog.h"
#include "ui_filetransferdialog.h"
#include <tcpnetworkconnection.h>

FileTransferDialog::FileTransferDialog(const std::size_t fileChunk,
                                       QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FileTransferDialog)
    , m_filePath{}
    , m_fileCheckSum{}
    , m_fileName{}
    , m_ip{}
    , m_port{}
    , m_fileSize(0)
    , m_fileChunk(fileChunk)    /*init fileChunk size*/
    , m_blockNumber(0)
{
    ui->setupUi(this);

    /*server not connected*/
    ui->send_button->setDisabled(true);
}

FileTransferDialog::~FileTransferDialog()
{
    delete ui;
}

bool FileTransferDialog::validateFile(const QString &file){
    QFileInfo info(file);
    if(!info.isFile() || info.isReadable()){
        return false;
    }

    m_filePath = file;
    m_fileSize = info.size();
    m_fileName = info.fileName();

    qDebug() << "File Name: " << m_fileName << " Has Been Loaded!\n"
             << "File Size = " << m_fileSize;
    return true;
}

void FileTransferDialog::updateProgressBar(const std::size_t fileSize){
    ui->progressBar->setRange(0, fileSize);
    ui->progressBar->setValue(0);
}

std::size_t FileTransferDialog::calculateBlockNumber(const std::size_t totalSize,
                                                     const std::size_t chunkSize){
    return static_cast<size_t>(std::ceil(static_cast<double>(totalSize) / chunkSize));
}

void FileTransferDialog::setServerInfo(const QString &ip, const QString &port){
    m_ip  = ip;
    m_port = port;
}

void FileTransferDialog::on_open_file_button_clicked(){

    ui->send_button->setDisabled(true);
    const auto fileName = QFileDialog::getOpenFileName(
        nullptr,
        "Open File",
        "",
        "Text Files (*.txt);;Images (*.png *.jpg);;PDF Files (*.pdf);;All Files (*)"
    );

    if(fileName.isEmpty()){
        /*filename not loaded, do not think about upload*/
        return;
    }

    if(!validateFile(m_filePath)){
        /*maybe its not a file and can not be read*/
        return;
    }

    /*more than 4GB*/
    if(m_fileSize > FOUR_GB){
        return;
    }

    /*init progress bar*/
    updateProgressBar(m_fileSize);

    /*update ui display*/
    ui->file_path->setText(m_filePath);
    ui->file_size_display->setText(QString::number(m_fileSize) + " byte");
    ui->send_button->setDisabled(false);
}

void FileTransferDialog::on_send_button_clicked(){

    ui->send_button->setDisabled(true);

    /*use md5 to mark this file*/
    QCryptographicHash hash(QCryptographicHash::Md5);

    QFile file(m_filePath);
    if(!file.open(QIODevice::ReadOnly)){
        qDebug() << "Cannot User ReadOnly To Open File";
        return;
    }

    /*try to hash the whole file and generate a unique record*/
    if(!hash.addData(&file)){
        qDebug() << "Hashing File Failed!";
        return;
    }

    m_fileCheckSum = hash.result().toHex();

    /*there is how many blocks(loops) we need to parse the file*/
    m_blockNumber =  calculateBlockNumber(m_fileSize, m_fileChunk);

    /*seek head of the file*/
    file.seek(SEEK_SET);

    /*record current msg seq*/
    std::size_t cur_seq = 1;

    /*start to parse the file*/
    while(!file.atEnd()){
         QJsonObject obj;

         auto bytes_transferr = (cur_seq != m_blockNumber) ?
            m_fileChunk : m_fileSize - (cur_seq - 1) * m_fileChunk;

         if(!bytes_transferr){
             break;
         }

        /*get a chunk size*/
        QByteArray buffer(file.read(bytes_transferr));

        obj["filename"] = m_fileName;
        obj["checksum"] = QString(m_fileCheckSum);
        obj["cur_size"] = QString::number(bytes_transferr + (cur_seq - 1) * m_fileChunk);
        obj["file_size"] = QString::number(m_fileSize);
        obj["block"] = QString(buffer.toBase64());
        obj["cur_seq"] = QString::number(cur_seq);
        obj["last_seq"] = QString::number(m_blockNumber);

        /*End of Transmission*/
        if(cur_seq == m_blockNumber){
            obj["EOF"] = QString::number(1);
        }

        QJsonDocument doc(obj);
        auto json_data = doc.toJson(QJsonDocument::Compact);

        SendNode<QByteArray, std::function<uint16_t(uint16_t)>> send_buffer(
            static_cast<uint16_t>(ServiceType::SERVICE_FILEUPLOADREQUEST),
            json_data, [](auto x) { return qToBigEndian(x); });

        TCPNetworkConnection::get_instance()->send_data(std::move(send_buffer));

        ++cur_seq;
    }

    file.close();

    ui->send_button->setDisabled(false);
}


void FileTransferDialog::on_connect_server_clicked(){
    emit

    ui->connect_server->setDisabled(true);
    ui->send_button->setDisabled(false);
}

