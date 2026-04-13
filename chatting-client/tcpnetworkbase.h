#pragma once
#ifndef TCPNETWORKBASE_H
#define TCPNETWORKBASE_H

#include "def.hpp"
#include <MsgNode.hpp>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QUrl>
#include <functional>
#include <queue>
#include <singleton.hpp>

enum class MsgType;

enum class TargetServer { CHATTINGSERVER, RESOURCESSERVER };

class TCPNetworkBase : public QObject {
  Q_OBJECT
public:
  explicit TCPNetworkBase(const MsgNodeType _type, const TargetServer _server,
                          QObject *parent = nullptr);

  virtual ~TCPNetworkBase();
  void send_buffer(ServiceType type, QJsonObject &&obj);

protected:
  struct RecvInfo {
    RecvInfo() = default;
    uint16_t _id = 0;
    uint16_t _length = 0;
    QByteArray _msg;
  };

  using Callbackfunction = std::function<void(QJsonObject &&)>;
  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;
  using RecvNodeType = RecvNode<QByteArray, ByteOrderConverter>;

  void registerSocketSignal();
  void registerErrorHandling();
  void readyReadHandler(const uint16_t id, QJsonObject &&obj);

  virtual void registerNetworkEvent() = 0;
  virtual void registerCallback() = 0;
  virtual void registerMetaType() = 0;

  /*establish tcp socket with server*/
  QTcpSocket m_socket;

  /*according to service type to execute callback*/
  std::map<ServiceType, Callbackfunction> m_callbacks;

private:
  void setupDataRetrieveEvent(QTcpSocket &socket, RecvInfo &received,
                              RecvNodeType &buffer);

signals:
  /*login to server failed*/
  void signal_login_failed(ServiceStatus status);

  void signal_block_send(qint64 size);

  /*return connection status to login class*/
  void signal_connection_status(bool status);

  void signal_connect2_server();
  void signal_terminate_server();

  /*
   * Send signals to a unified slot function for processing,
   * implementing a queue mechanism and ensuring thread safety.
   */
  void signal_send_message(std::shared_ptr<SendNodeType> data);

private slots:
  void shutdown(bool connected = true);

  /*Send signals to a unified slot function for processing,
   * implementing a queue mechanism and ensuring thread safety.
   */
  void slot_send_message(std::shared_ptr<SendNodeType> data);

  virtual void slot_terminate_server() {};
  virtual void slot_connect2_server() {};

private:
  const MsgNodeType m_type;
  const TargetServer m_server;

  qint64 m_bytes_have_been_written{};

  // the qbytearray we are processing right now!
  QByteArray m_curr_processing{};

  // is there any bytearray we are processing right now
  bool m_pending_flag = false;

  // queue
  std::queue<QByteArray> m_queue;

  /*create a connection buffer to store the data transfer from server*/
  RecvInfo m_info;

  std::unique_ptr<RecvNodeType> m_buffer;
};

Q_DECLARE_METATYPE(TargetServer)

#endif // TCPNETWORKBASE_H
