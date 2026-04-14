#include "filetcpnetwork.h"
#include <logicmethod.h>
#include <useraccountmanager.hpp>

FileTCPNetwork::FileTCPNetwork()
    : TCPNetworkBase{MsgNodeType::MSGNODE_FILE_TRANSFER,
                     TargetServer::RESOURCESSERVER} {
  /*register meta type, it should be done BEFORE connect*/
  registerMetaType();

  /*callbacks should be registered at first(before signal)*/
  registerCallback();

  /*register connection event*/
  registerNetworkEvent();
}

void FileTCPNetwork::send_download_request(std::shared_ptr<FileTransferDesc> info){
    QJsonObject obj;

    obj["uuid"] = UserAccountManager::get_instance()->get_uuid();
    obj["filename"] = info->filename;
    obj["filepath"] = info->filePath;
    obj["cur_seq"] =  QString::number(info->curr_sequence);

    /*negotiated transfer block size*/
    obj["current_block_size"] = QString::number(4096);  //4KB by default

    //how many bytes have been transferred till now?
    obj["transfered_size"] = QString::number(info->transfered_size);

    if( info->curr_sequence != 1 ){
        obj["last_seq"] = QString::number(info->last_sequence);
        obj["total_size"] = QString::number(info->total_size);
        obj["checksum"] = info->checksum;
    }
    obj["EOF"] = QString::number(info->isEOF);

    FileTCPNetwork::get_instance()->send_buffer(
        info->curr_sequence == 1 ?
            ServiceType::SERVICE_INITFILEFETCHINGREQUEST :
            ServiceType::SERVICE_FILEDOWNLOADREQUEST,
        std::move(obj));
}

void FileTCPNetwork::registerNetworkEvent() {}

void FileTCPNetwork::registerCallback() {
    auto uploadcallback = [this](QJsonObject &&json){

        [[maybe_unused]] auto filename = json["filename"].toString();
        [[maybe_unused]] auto filepath = json["filepath"].toString();
        [[maybe_unused]] auto checksum = json["checksum"].toString();
        [[maybe_unused]] auto curr_seq = json["curr_seq"].toString().toUInt();
        [[maybe_unused]] auto last_seq = json["last_seq"].toString().toUInt();
        [[maybe_unused]] auto curr_size = json["curr_size"].toString().toUInt();
        [[maybe_unused]] auto total_size = json["total_size"].toString().toUInt();
        [[maybe_unused]] auto eof = json["EOF"].toBool();

        emit signal_breakpoint_upload(std::make_shared<FileTransferDesc>(
                filename,
                checksum,
                filepath,
                curr_seq,
                last_seq,
                eof,
                curr_size,
                total_size));
    };

    m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
        ServiceType::SERVICE_FILEUPLOADRESPONSE, [this, uploadcallback](QJsonObject &&json) {

            if (!json.contains("error")) {
                qDebug() << "Json Parse Error!";
                return;
            }
            if (json["error"].toInt() !=
                static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
                qDebug() << "File Upload Failed!!";
                return;
            }

            uploadcallback(std::move(json));
        }));

    m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
        ServiceType::SERVICE_FILECHECKUPLOADPROGRESSRESPONSE, [this, uploadcallback](QJsonObject &&json) {
            if (!json.contains("error")) {
                qDebug() << "Json Parse Error!";
                return;
            }
            if (json["error"].toInt() !=
                static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
                qDebug() << "File Upload Failed!!";
                return;
            }

            LogicMethod::get_instance()->setPause(true);

            uploadcallback(std::move(json));
        }));

    m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
        ServiceType::SERVICE_FILEDOWNLOADRESPONSE, [this, uploadcallback](QJsonObject &&json) {
            if (!json.contains("error")) {
                qDebug() << "Json Parse Error!";
                return;
            }
            if (json["error"].toInt() !=
                static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {

                switch(json["error"].toInt()){
                    case static_cast<int>(ServiceStatus::FILE_NOT_FOUND):
                        qDebug() << "File Not Exist!";
                        break;
                }

                return;
            }

            [[maybe_unused]] auto filename = json["filename"].toString();
            [[maybe_unused]] auto filepath = json["filepath"].toString();
            [[maybe_unused]] auto checksum = json["checksum"].toString();

            /*this is the data area!*/
            [[maybe_unused]] auto data_block = json["block"].toString();

            [[maybe_unused]] auto curr_seq = json["curr_seq"].toString().toUInt();
            [[maybe_unused]] auto last_seq = json["last_seq"].toString().toUInt();

            [[maybe_unused]] auto current_block_size = json["current_block_size"].toString().toUInt();
            [[maybe_unused]] auto transfered_size = json["transfered_size"].toString().toUInt();
            [[maybe_unused]] auto total_size = json["total_size"].toString().toUInt();

            [[maybe_unused]] auto eof = json["EOF"].toBool();

            emit signal_breakpoint_download(
                std::make_shared<FileTransferDesc>(filename,
                                                   checksum,
                                                   filepath,
                                                   curr_seq,
                                                   last_seq,
                                                   eof,
                                                   transfered_size,
                                                   total_size,
                                                   TransferDirection::Download),

                /*base64 => normal*/
                QByteArray::fromBase64(data_block.toUtf8()),
                current_block_size
            );
        }));
}

void FileTCPNetwork::registerMetaType() {
    qRegisterMetaType<std::shared_ptr<FileTransferDesc>>(
        "std::shared_ptr<FileTransferDesc>");
}

void FileTCPNetwork::slot_terminate_server() {}

void FileTCPNetwork::slot_connect2_server() {
  qDebug() << "Connecting to Resources Server"
           << "\nuuid = " << ResourceStorageManager::get_instance()->get_uuid()
           << "\nhost = " << ResourceStorageManager::get_instance()->get_host()
           << "\nport = " << ResourceStorageManager::get_instance()->get_port()
           << '\n';

  m_socket.connectToHost(
      ResourceStorageManager::get_instance()->get_host(),
      ResourceStorageManager::get_instance()->get_port().toUShort());
}
