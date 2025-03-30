#include "tcpnetworkconnection.h"
#include "UserFriendRequest.hpp"
#include "UserNameCard.h"
#include "useraccountmanager.hpp"
#include <ChattingHistory.hpp>
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <msgtextedit.h>
#include <qjsonarray.h>
#include <resourcestoragemanager.h>

TCPNetworkConnection::TCPNetworkConnection()
    : m_chatting_buffer([](auto x) { return qFromBigEndian(x); })
    , m_resources_buffer([](auto x) { return qFromBigEndian(x); }){

  /*callbacks should be registered at first(before signal)*/
  registerCallback();

  /*register socket connect & disconnect & data ready signals */
  registerSocketSignal();

  /*setup socket error handling slot*/
  registerErrorHandling();

  /*register connection event*/
  registerNetworkEvent();
}

TCPNetworkConnection::~TCPNetworkConnection() {

    if(m_chatting_server_socket.isOpen()){
        m_chatting_server_socket.close();
    }
    if(m_resources_server_socket.isOpen()){
        m_resources_server_socket.close();
    }

}

void TCPNetworkConnection::registerNetworkEvent() {
  connect(this, &TCPNetworkConnection::signal_connect2_chatting_server, this,
          &TCPNetworkConnection::slot_connect2_chatting_server);

    connect(this, &TCPNetworkConnection::signal_connect2_resources_server, this,
            &TCPNetworkConnection::slot_connect2_resources_server);

  connect(this, &TCPNetworkConnection::signal_terminate_resources_server, this,
          &TCPNetworkConnection::slot_terminate_resources_server);
}

void TCPNetworkConnection::registerSocketSignal() {
  /*connected to server successfully*/
  connect(&m_chatting_server_socket, &QTcpSocket::connected, [this]() {
    qDebug() << "connected to chatting server successfully";
    emit signal_connection_status(true);
  });

    connect(&m_resources_server_socket, &QTcpSocket::connected, [this]() {
        qDebug() << "connected to resources server successfully";
        emit signal_connection_status(true);
    });

  /*server disconnected*/
  connect(&m_chatting_server_socket, &QTcpSocket::disconnected, [this]() {
    qDebug() << "server chatting disconnected";
    emit signal_connection_status(false);
  });

    connect(&m_resources_server_socket, &QTcpSocket::disconnected, [this]() {
        qDebug() << "server resources disconnected";
        emit signal_connection_status(false);
    });


  /*receive data from server*/
    setupDataRetrieveEvent(m_chatting_server_socket, m_chatting_info, m_chatting_buffer);
    setupDataRetrieveEvent(m_resources_server_socket, m_resource_info, m_resources_buffer);
}

void TCPNetworkConnection::registerErrorHandling() {
  connect(
      &m_chatting_server_socket,
      QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
      [this]([[maybe_unused]] QTcpSocket::SocketError socketErr) {
        qDebug() << "Connection To chatting server Tcp error: " << m_chatting_server_socket.errorString();
        emit signal_connection_status(false);
      });

    connect(
        &m_resources_server_socket,
        QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
        [this]([[maybe_unused]] QTcpSocket::SocketError socketErr) {
            qDebug() << "Connection To resources server Tcp error: " << m_resources_server_socket.errorString();
            emit signal_connection_status(false);
        });
}

void TCPNetworkConnection::setupDataRetrieveEvent(QTcpSocket &socket,
                                                  RecvInfo &received,
                                                  RecvNode<QByteArray, std::function<uint16_t (uint16_t)> > &buffer)
{

    connect(&socket, &QTcpSocket::readyRead, [&socket, &received, &buffer, this]() {
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

            /*to prevent app crash due to callback is not exists*/
            try {
                m_callbacks[static_cast<ServiceType>(received._id)](
                    std::move(json_obj.object()));
            } catch (const std::exception &e) {
                qDebug() << e.what();
            }
        }
    });
}

void TCPNetworkConnection::registerCallback() {
  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_LOGINRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          emit signal_login_failed(ServiceStatus::JSONPARSE_ERROR);
          return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Login Server Error!";
          emit signal_login_failed(
              static_cast<ServiceStatus>(json["error"].toInt()));
          return;
        }

        /*store current user info inside account manager*/
        UserAccountManager::get_instance()->setUserInfo(
            std::make_shared<UserNameCard>(
                json["uuid"].toString(), json["avator"].toString(),
                json["username"].toString(), json["nickname"].toString(),
                json["description"].toString(),
                static_cast<Sex>(json["sex"].toInt())));

        emit signal_switch_chatting_dialog();

        /*is there anyone send friending request to this person*/
        if (json.contains("FriendRequestList")) {
          UserAccountManager::get_instance()->appendArrayToList(
              TargetList::REQUESTLIST, json["FriendRequestList"].toArray());

          /*server be able to send friend request list to this client*/
          emit signal_init_friend_request_list();
        }

        /*is there existing authenticated friend to this person*/
        if (json.contains("AuthFriendList")) {
          UserAccountManager::get_instance()->appendArrayToList(
              TargetList::FRIENDLIST, json["AuthFriendList"].toArray());

          /*server be able to send authenticate friend list to this client*/
          emit signal_init_auth_friend_list();
        }
      }));

  /*Client search username and server return result back*/
  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_SEARCHUSERNAMERESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";

          emit signal_search_username(std::nullopt,
                                      ServiceStatus::JSONPARSE_ERROR);
          return;
        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Login Server Error!";

          emit signal_search_username(
              std::nullopt, static_cast<ServiceStatus>(json["error"].toInt()));
          return;
        } else {
          auto uuid = json["uuid"].toString();
          auto username = json["username"].toString();
          auto nickname = json["nickname"].toString();
          auto avator = json["avator"].toString();
          auto description = json["description"].toString();
          auto sex = static_cast<Sex>(json["sex"].toInt());

          qDebug() << "Retrieve Data From Server of uuid = " << uuid << ":"
                   << "username = " << username << '\n'
                   << "nickname = " << nickname << '\n'
                   << "avator = " << avator << '\n'
                   << "description = " << description << '\n';

          emit signal_search_username(
              std::make_shared<UserNameCard>(uuid, avator, username, nickname,
                                             description, sex),
              ServiceStatus::SERVICE_SUCCESS);
        }
      }));

  /*the person who started to send friend request to other*/
  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FRIENDSENDERRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          emit signal_sender_response(false);
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Friend Request Sent Failed! Because Of Error Code = "
                   << json["error"].toInt() << '\n';
          emit signal_sender_response(false);
          return;
        }

        qDebug() << "Friend Request Sent Successfully!";
        emit signal_sender_response(true);
      }));

  /*the person who is going to confirm a friend request*/
  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FRIENDCONFIRMRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          emit signal_confirm_response(false);
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Friend Confirm Sent Failed! Because Of Error Code = "
                   << json["error"].toInt() << '\n';
          emit signal_confirm_response(false);
          return;
        }

        qDebug() << "Friend Confirm Sent Successfully!";
        emit signal_confirm_response(true);
      }));

  /*the person who is going to receive friend request*/
  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FRIENDREINCOMINGREQUEST, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          emit signal_incoming_friend_request(std::nullopt);
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Receive Friend Request Send Failed!";

          emit signal_incoming_friend_request(std::nullopt);
          return;
        }

        qDebug() << "Receive Friend Request Send Successfully!";

        auto request = std::make_shared<UserFriendRequest>(
            json["src_uuid"].toString(), json["dst_uuid"].toString(),
            json["src_nickname"].toString(), json["src_message"].toString(),
            json["src_avator"].toString(), json["src_username"].toString(),
            json["src_desc"].toString(),
            static_cast<Sex>(json["src_sex"].toInt()));

        emit signal_incoming_friend_request(request);
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
      [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          emit signal_add_authenticate_friend(std::nullopt);
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug()
              << "Friending On biddirectional failed! Because Of Error Code = "
              << json["error"].toInt() << '\n';
          emit signal_add_authenticate_friend(std::nullopt);
          return;

        } else {
          auto uuid = json["friend_uuid"].toString();
          auto username = json["friend_username"].toString();
          auto nickname = json["friend_nickname"].toString();
          auto avator = json["friend_avator"].toString();
          auto description = json["friend_desc"].toString();
          auto sex = static_cast<Sex>(json["friend_sex"].toInt());

          qDebug() << "Retrieve Data From Server of uuid = " << uuid << ":"
                   << "username = " << username << '\n'
                   << "nickname = " << nickname << '\n'
                   << "avator = " << avator << '\n'
                   << "description = " << description << '\n';

          auto card = std::make_shared<UserNameCard>(
              uuid, avator, username, nickname, description, sex);

          /*add it to list in advance for user identity validation*/
          UserAccountManager::get_instance()->addItem2List(card);
          emit signal_add_authenticate_friend(card);
        }
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_TEXTCHATMSGRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Send Text Chat Msg failed! Because Of Error Code = "
                   << json["error"].toInt() << '\n';
          return;
        }
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_TEXTCHATMSGICOMINGREQUEST,
      [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";

          emit signal_incoming_text_msg(MsgType::TEXT, std::nullopt);
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Receive Incoming Text Chat Msg failed! Because Of Error "
                      "Code = "
                   << json["error"].toInt() << '\n';

          emit signal_incoming_text_msg(MsgType::TEXT, std::nullopt);
          return;

        } else {
          auto text_sender = json["text_sender"].toString();
          auto text_receiver = json["text_receiver"].toString();
          auto text_msg = json["text_msg"].toArray();

          qDebug() << "Retrieve Text Msg Data From Server of: "
                   << "src_uuid = " << text_sender << '\n'
                   << "dst_uuid = " << text_receiver << '\n'
                   << "text_msg = " << text_msg;

          emit signal_incoming_text_msg(
              MsgType::TEXT, std::make_shared<ChattingTextMsg>(
                                 text_sender, text_receiver, text_msg));
        }
      }));
}

void TCPNetworkConnection::slot_connect2_chatting_server() {

  qDebug() << "Connecting to Chatting Server"
           << "\nuuid = " << UserAccountManager::get_instance()->get_uuid()
           << "\nhost = " << UserAccountManager::get_instance()->get_host()
           << "\nport = " << UserAccountManager::get_instance()->get_port()
           << "\ntoken = " << UserAccountManager::get_instance()->get_token()
           << '\n';

  /*the successful or unsuccessful signal is going to generate in
   * signal<->slot*/
  m_chatting_server_socket.connectToHost(
      UserAccountManager::get_instance()->get_host(),
      UserAccountManager::get_instance()->get_port().toUShort());
}

void TCPNetworkConnection::slot_connect2_resources_server(){
    qDebug() << "Connecting to Resources Server"
             << "\nuuid = " << ResourceStorageManager::get_instance()->get_uuid()
             << "\nhost = " << ResourceStorageManager::get_instance()->get_host()
             << "\nport = " << ResourceStorageManager::get_instance()->get_port()
             << '\n';

    m_resources_server_socket.connectToHost(
        ResourceStorageManager::get_instance()->get_host(),
        ResourceStorageManager::get_instance()->get_port().toUShort());
}

void TCPNetworkConnection::slot_terminate_resources_server(){
    qDebug() << "Terminate From Resources Server\n";

    m_resources_server_socket.close();
}

void TCPNetworkConnection::send_data(
    SendNode<QByteArray, std::function<uint16_t(uint16_t)>> &&data, TargetServer tar) {

    if(tar == TargetServer::CHATTINGSERVER){
        m_chatting_server_socket.write(data.get_buffer());
    }
    else if(tar == TargetServer::RESOURCESSERVER){
        m_resources_server_socket.write(data.get_buffer());
    }
}
