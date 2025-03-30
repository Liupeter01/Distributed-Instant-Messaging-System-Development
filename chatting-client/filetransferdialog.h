#ifndef FILETRANSFERDIALOG_H
#define FILETRANSFERDIALOG_H

#include <QWidget>
#include <QByteArray>

#define GB_TO_BYTES(gb) ((gb) * 1024LL * 1024 * 1024)
#define FOUR_GB GB_TO_BYTES(4)

namespace Ui {
class FileTransferDialog;
}

class FileTransferDialog : public QWidget
{
    Q_OBJECT

public:
    explicit FileTransferDialog(const std::size_t fileChunk = 2048,
                                QWidget *parent = nullptr);

    virtual ~FileTransferDialog();

private:
    bool validateFile(const QString &file);
    void updateProgressBar(const std::size_t fileSize);

public:
    static std::size_t calculateBlockNumber(const std::size_t totalSize,
                                     const std::size_t chunkSize);

    void setServerInfo(const QString&ip, const QString&port);

private slots:
    /*open file*/
    void on_open_file_button_clicked();

    /*upload to server*/
    void on_send_button_clicked();

    /*connect to server*/
    void on_connect_server_clicked();

private:
    Ui::FileTransferDialog *ui;
    /*Server Info*/
    QString m_ip;
    QString m_port;

    /*chunk size and chunk number consist of this file*/
    std::size_t m_fileChunk = 2048;
    std::size_t m_blockNumber = 0;

    /*file basic info*/
    QString m_filePath;
    QString m_fileName;
    std::size_t m_fileSize;

    /*md5 checksum*/
    QByteArray m_fileCheckSum;
};

#endif // FILETRANSFERDIALOG_H
