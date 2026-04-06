#include "filetcpnetwork.h"
#include <resourcestoragemanager.h>

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

void FileTCPNetwork::registerNetworkEvent() {}

void FileTCPNetwork::registerCallback() {}

void FileTCPNetwork::registerMetaType() {}

void FileTCPNetwork::readyReadHandler(const uint16_t id, QJsonObject &&obj){

    emit signal_resources_logic_handler(id, obj);
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
