#include "tcpnetworkbase.h"
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <UserDef.hpp>
#include <logicmethod.h>
#include <msgtextedit.h>
#include <qjsonarray.h>
#include <tools.h>

TCPNetworkBase::TCPNetworkBase(const MsgNodeType _type,
                               const TargetServer _server, QObject *parent)
    : QObject{}, m_type(_type), m_server(_server), m_pending_flag(false),
      m_bytes_have_been_written(0), m_curr_processing{},
      m_buffer(std::make_unique<RecvNodeType>(ByteOrderConverter{}, _type)) {

  /*register socket connect & disconnect & data ready signals */
  registerSocketSignal();

  /*setup socket error handling slot*/
  registerErrorHandling();
}

TCPNetworkBase::~TCPNetworkBase() { emit signal_terminate_server(); }

void TCPNetworkBase::send_buffer(ServiceType type, QJsonObject &&obj) {

  QJsonDocument doc(std::move(obj));
  auto byte = doc.toJson(QJsonDocument::Compact);

  std::shared_ptr<SendNodeType> buffer = std::make_shared<SendNodeType>(
      static_cast<uint16_t>(type), byte, ByteOrderConverterReverse{}, m_type);

  /*after connection to server, send TCP request*/
  emit signal_send_message(buffer);
}

void TCPNetworkBase::registerSocketSignal() {

  connect(&m_socket, &QTcpSocket::connected, this,
          [this]() { emit signal_connection_status(true); });

  connect(&m_socket, &QTcpSocket::disconnected, this,
          [this]() { emit signal_connection_status(false); });

  // messages sending callback signal & slot!
  connect(&m_socket, &QTcpSocket::bytesWritten, this, [this](qint64 bytes) {
    // progress display signal for dialog Bar widget display
    emit signal_block_send(bytes);

    m_bytes_have_been_written += bytes;

    // Not Finished Yet
    if (m_bytes_have_been_written < m_curr_processing.size()) {

      // split the rest part of the data!
      auto new_piece = m_curr_processing.mid(m_bytes_have_been_written);
      m_socket.write(new_piece);
      return;
    }

    // clear some data;
    m_curr_processing.clear();
    m_bytes_have_been_written = 0;

    // sending complete & queue is empty
    if (m_queue.empty()) {
      m_pending_flag = false;

    } else {
      m_pending_flag = true;
      m_curr_processing = m_queue.front();
      m_queue.pop();

      m_socket.write(m_curr_processing);
    }
  });

  setupDataRetrieveEvent(m_socket, m_info, *m_buffer);

  connect(this, &TCPNetworkBase::signal_send_message, this,
          &TCPNetworkBase::slot_send_message);

  connect(this, &TCPNetworkBase::signal_connect2_server, this,
          &TCPNetworkBase::slot_connect2_server);

  connect(this, &TCPNetworkBase::signal_terminate_server, this,
          &TCPNetworkBase::slot_terminate_server);

  connect(this, &TCPNetworkBase::signal_connection_status, this,
          &TCPNetworkBase::shutdown);
}

void TCPNetworkBase::registerErrorHandling() {

  connect(
      &m_socket,
      QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
      this, [this]([[maybe_unused]] QTcpSocket::SocketError socketErr) {
        switch (socketErr) {
        case QTcpSocket::ConnectionRefusedError:
          qDebug() << "NetworkBase QTcpSocket::ConnectionRefusedError\n";
          break;
        case QTcpSocket::RemoteHostClosedError:
          qDebug() << "NetworkBase QTcpSocket::RemoteHostClosedError\n";
          emit signal_connection_status(false);
          break;
        case QTcpSocket::HostNotFoundError:
          qDebug() << "NetworkBase QTcpSocket::HostNotFoundError\n";
          break;
        case QTcpSocket::NetworkError:
          qDebug() << "NetworkBase QTcpSocket:::NetworkError\n";
          break;
        case QTcpSocket::SocketTimeoutError:
          qDebug() << "NetworkBase QTcpSocket::SocketTimeoutError\n";
          break;
        }
      });
}

void TCPNetworkBase::readyReadHandler(const uint16_t id, QJsonObject &&obj) {
  try {
    m_callbacks[static_cast<ServiceType>(id)](std::move(obj));
  } catch (const std::exception &e) {
    qDebug() << e.what();
  }
}

void TCPNetworkBase::shutdown(bool connected) {

  if (connected)
    return;
  if (m_socket.isOpen())
    m_socket.close();
}

void TCPNetworkBase::setupDataRetrieveEvent(QTcpSocket &socket,
                                            RecvInfo &received,
                                            RecvNodeType &buffer) {
  connect(&socket, &QTcpSocket::readyRead, this,
          [&socket, &received, &buffer, this]() {
            while (socket.bytesAvailable() > 0) {
              QByteArray array = socket.readAll(); // Read all available data

              /*
               * Ensure the received data is large enough to include the header
               * if no enough data, then continue waiting
               */
              while (array.size() >= buffer.get_header_length()) {

                // Check if we are still receiving the header
                if (buffer.check_header_remaining()) {

                  /*
                   * Take the necessary portion from the array for the header
                   * Insert the header data into the buffer
                   */
                  buffer._buffer = array.left(buffer.get_header_length());
                  buffer.update_pointer_pos(buffer.get_header_length());

                  received._id = buffer.get_id().value();
                  received._length = buffer.get_length().value();

                  // Clear the header part from the array
                  array.remove(0, buffer.get_header_length());
                }

                if (array.size() < received._length) {
                  return;
                }

                // If we have remaining data in array, treat it as body
                if (buffer.check_body_remaining()) {

                  std::memcpy(buffer.get_body_base(), array.data(),
                              received._length);

                  buffer.update_pointer_pos(received._length);

                  /*
                   * Clear the body part from the array
                   * Maybe there are some other data inside
                   */
                  array.remove(0, received._length);
                }
              }

              // Now, both the header and body are fully received
              received._msg = buffer.get_msg_body().value();

              // Debug output to show the received message
              qDebug() << "msg_id = " << received._id << "\n"
                       << "msg_length = " << received._length << "\n"
                       << "msg_data = " << received._msg << "\n";

              // Clear the buffer for the next message
              buffer.clear();

              /*parse it as json*/
              QJsonDocument json_obj = QJsonDocument::fromJson(received._msg);
              if (json_obj.isNull()) { // converting failed
                // journal log system
                qDebug() << __FILE__ << "[FATAL ERROR]: json object is null!\n";
                emit signal_login_failed(ServiceStatus::JSONPARSE_ERROR);
                return;
              }

              if (!json_obj.isObject()) {
                // journal log system
                qDebug() << __FILE__ << "[FATAL ERROR]: json object is null!\n";
                emit signal_login_failed(ServiceStatus::JSONPARSE_ERROR);
                return;
              }

              readyReadHandler(received._id, std::move(json_obj.object()));
            }
          });
}

void TCPNetworkBase::slot_send_message(std::shared_ptr<SendNodeType> data) {
  // another bytearray is now being processed by the kernel network!
  if (m_pending_flag) {

    // guarantee the messages are processed one after another in sequence
    m_queue.push(std::move(data->get_buffer()));
    return;
  }

  m_curr_processing = std::move(data->get_buffer());
  m_pending_flag = true;
  m_bytes_have_been_written = 0;

  [[maybe_unused]] qint64 bytes_written = m_socket.write(m_curr_processing);
  if (!bytes_written) {
    qDebug() << "Network: EWORLDBLOCK/EAGAIN\n"
                "As a result, Messages are not sent at this time, waiting for "
                "next term!\n";
  }
}
