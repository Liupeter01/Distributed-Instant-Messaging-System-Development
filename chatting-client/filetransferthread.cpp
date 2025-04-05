#include "filetransferthread.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <tcpnetworkconnection.h>
#include <logicmethod.h>

FileTransferThread::FileTransferThread(QObject *parent)
    : QObject{parent}
    , m_thread(new QThread(this))
{
    moveToThread(m_thread);

    registerSignal();

    m_thread->start();
}

FileTransferThread::~FileTransferThread()
{
    m_thread->quit();
    m_thread->wait();
    m_thread->deleteLater();
}

std::size_t
FileTransferThread::calculateBlockNumber(const std::size_t totalSize,
                                         const std::size_t chunkSize) {
    return static_cast<size_t>(
        std::ceil(static_cast<double>(totalSize) / chunkSize));
}

void FileTransferThread::registerSignal()
{
    connect(this, &FileTransferThread::signal_start_file_transmission,
            this, &FileTransferThread::slot_start_file_transmission
            );

    connect(this, &FileTransferThread::signal_send_next_block,
            this, &FileTransferThread::slot_send_next_block);

    connect(LogicMethod::get_instance().get(),
            &LogicMethod::signal_data_transmission_status, this,
            [this](const QString &filename, const std::size_t curr_seq,
                   const std::size_t curr_size, const std::size_t total_size) {

                m_curSeq = curr_seq + 1;
                accumulate_transferred = curr_size;

                emit signal_send_next_block();
            });
}

/*update seq and accumulate size*/
void FileTransferThread::slot_send_next_block()
{
    if (m_curSeq > m_totalBlocks || m_file.atEnd()) {
        m_file.close();
        return;
    }

    std::size_t bytes_transferred_curr_sequence =
        (m_curSeq != m_totalBlocks)
            ? m_fileChunk
            : m_fileSize - (m_curSeq - 1) * m_fileChunk;

    QByteArray buffer = m_file.read(bytes_transferred_curr_sequence);
    if (buffer.isEmpty()) {
        qDebug() << "transferred bytes = 0 in seq = " << m_curSeq;
        return;
    }

    QJsonObject obj;
    obj["filename"] = m_fileName;
    obj["checksum"] = m_fileCheckSum;
    obj["cur_seq"] = QString::number(m_curSeq);
    obj["last_seq"] = QString::number(m_totalBlocks);
    obj["cur_size"] = QString::number(accumulate_transferred + bytes_transferred_curr_sequence);
    obj["file_size"] = QString::number(m_fileSize);
    obj["block"] = QString(buffer.toBase64());
    obj["EOF"] = (m_curSeq == m_totalBlocks) ? "1" : "0";

    QJsonDocument doc(obj);
    QByteArray json_data = doc.toJson(QJsonDocument::Compact);

    auto send_buffer = std::make_shared<SendNodeType>(
        static_cast<uint16_t>(ServiceType::SERVICE_FILEUPLOADREQUEST),
        json_data, ByteOrderConverterReverse{},
        MsgNodeType::MSGNODE_FILE_TRANSFER);

    TCPNetworkConnection::get_instance()->send_sequential_data_f(
        send_buffer, TargetServer::RESOURCESSERVER);

    //Maybe use another computer?
    QThread::msleep(10);
}

void FileTransferThread::slot_start_file_transmission(const QString&fileName,
                                                      const QString&filePath,
                                                      const std::size_t fileChunk)
{
    m_fileName = fileName;
    m_filePath = filePath;
    m_fileChunk = fileChunk;
    m_curSeq = 1;

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot User ReadOnly To Open File";
        return;
    }

    m_fileSize = m_file.size();
    m_totalBlocks = calculateBlockNumber(m_fileSize, m_fileChunk);

    /*use md5 to mark this file*/
    QCryptographicHash hash(QCryptographicHash::Md5);

    /*
   * try to hash the whole file and generate a unique record
   * WARNING: after call addData, the file pointer will move to
   * another position rather than current pos
   */
    if (!hash.addData(&m_file)) {
        qDebug() << "Hashing File Failed!";
        return;
    }

    m_fileCheckSum = hash.result().toHex();
    m_file.seek(0);

    emit signal_send_next_block();
}
