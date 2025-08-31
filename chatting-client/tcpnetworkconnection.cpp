#include "tcpnetworkconnection.h"
#include "useraccountmanager.hpp"
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <UserDef.hpp>
#include <logicmethod.h>
#include <magic_enum.hpp>
#include <msgtextedit.h>
#include <qjsonarray.h>
#include <resourcestoragemanager.h>
#include <tools.h>

TCPNetworkConnection::TCPNetworkConnection()
    : m_chatting_buffer(std::make_unique<RecvNodeType>(ByteOrderConverter{})),
      m_resources_buffer(std::make_unique<RecvNodeType>(
          ByteOrderConverter{}, MsgNodeType::MSGNODE_FILE_TRANSFER)) {

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

  terminate_chatting_server();
  slot_terminate_resources_server();
}

void TCPNetworkConnection::registerNetworkEvent() {
  connect(this, &TCPNetworkConnection::signal_connect2_chatting_server, this,
          &TCPNetworkConnection::slot_connect2_chatting_server);

  connect(this, &TCPNetworkConnection::signal_connect2_resources_server, this,
          &TCPNetworkConnection::slot_connect2_resources_server);

  connect(this, &TCPNetworkConnection::signal_teminate_chatting_server, this,
          &TCPNetworkConnection::slot_terminate_chatting_server);

  connect(this, &TCPNetworkConnection::signal_terminate_resources_server, this,
          &TCPNetworkConnection::slot_terminate_resources_server);

  connect(this, &TCPNetworkConnection::signal_send_message, this,
          &TCPNetworkConnection::slot_send_message);

  connect(this, &TCPNetworkConnection::signal_resources_logic_handler,
          LogicMethod::get_instance().get(),
          &LogicMethod::signal_resources_logic_handler);

  connect(this, &TCPNetworkConnection::signal_logout_status, this,
          &TCPNetworkConnection::terminate_chatting_server);
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

    emit signal_logout_status(true);
    emit signal_connection_status(false);
  });

  connect(&m_resources_server_socket, &QTcpSocket::disconnected, [this]() {
    qDebug() << "server resources disconnected";
    emit signal_connection_status(false);
  });

  connect(&m_resources_server_socket, &QTcpSocket::bytesWritten,
          [this](qint64 bytes) { emit signal_block_send(bytes); });

  /*receive data from server*/
  setupChattingDataRetrieveEvent(m_chatting_server_socket, m_chatting_info,
                                 *m_chatting_buffer);
  setupResourcesDataRetrieveEvent(m_resources_server_socket, m_resource_info,
                                  *m_resources_buffer);
}

void TCPNetworkConnection::registerErrorHandling() {
  connect(
      &m_chatting_server_socket,
      QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
      [this]([[maybe_unused]] QTcpSocket::SocketError socketErr) {
        qDebug() << "Connection To chatting server Tcp error: "
                 << m_chatting_server_socket.errorString();
        emit signal_connection_status(false);
        emit signal_logout_status(true);
      });

  connect(
      &m_resources_server_socket,
      QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
      [this]([[maybe_unused]] QTcpSocket::SocketError socketErr) {
        qDebug() << "Connection To resources server Tcp error: "
                 << m_resources_server_socket.errorString();
        emit signal_connection_status(false);
      });
}

void TCPNetworkConnection::setupChattingDataRetrieveEvent(
    QTcpSocket &socket, RecvInfo &received, RecvNodeType &buffer) {

  connect(&socket, &QTcpSocket::readyRead,
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

              try {
                m_callbacks[static_cast<ServiceType>(received._id)](
                    std::move(json_obj.object()));
              } catch (const std::exception &e) {
                qDebug() << e.what();
              }
            }
          });
}

void TCPNetworkConnection::setupResourcesDataRetrieveEvent(
    QTcpSocket &socket, RecvInfo &received, RecvNodeType &buffer) {
  connect(
      &socket, &QTcpSocket::readyRead, [&socket, &received, &buffer, this]() {
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
          // qDebug() << "msg_id = " << received._id << "\n"
          //          << "msg_length = " << received._length << "\n"
          //          << "msg_data = " << received._msg << "\n";

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

          /*forward resources server's message to a standlone logic thread*/
          emit signal_resources_logic_handler(received._id, json_obj.object());
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

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_LOGOUTRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          emit signal_logout_status(true);
          return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Login Server Error!";
          emit signal_logout_status(true);
          return;
        }

        /*if resources dialog still open, then shut it down*/

        /*if chattingdlgmainframe still open. shut it down*/
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
          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug()
              << "Friending On biddirectional failed! Because Of Error Code = "
              << json["error"].toInt() << '\n';
          return;

        } else {

          if (!json["hello_msg"].isArray()) {
            qDebug() << "Try to parse hello_msg failed!";
            return;
          }

          auto uuid = json["friend_uuid"].toString();
          auto username = json["friend_username"].toString();
          auto nickname = json["friend_nickname"].toString();
          auto avator = json["friend_avator"].toString();
          auto description = json["friend_desc"].toString();
          auto sex = static_cast<Sex>(json["friend_sex"].toInt());

          auto thread_id = json["thread_id"].toString();
          auto hello_msg_arr = json["hello_msg"].toArray();

          auto chat_type =
              (json["chat_type"].toString() == "PRIVATE" ? UserChatType::PRIVATE
                                                         : UserChatType::GROUP);

          std::vector<std::shared_ptr<FriendingConfirmInfo>> list;
          for (const auto &item : hello_msg_arr) {
            if (!item.isObject()) {
              continue;
            }

            auto obj = item.toObject();
            auto msg_id = obj["msg_id"].toString();
            auto message_sender = obj["msg_sender"].toString();
            auto message_receiver = obj["msg_receiver"].toString();
            auto msg_content = obj["msg_content"].toString();
            auto msg_type = static_cast<MsgType>(obj["msg_type"].toInt());

            list.push_back(std::make_shared<FriendingConfirmInfo>(
                msg_type, thread_id, msg_id, message_sender, message_receiver,
                msg_content));
          }

          qDebug() << "Retrieve Data From Server of uuid = " << uuid << ":"
                   << "username = " << username << '\n'
                   << "nickname = " << nickname << '\n'
                   << "avator = " << avator << '\n'
                   << "description = " << description << '\n';

          auto namecard = std::make_shared<UserNameCard>(
              uuid, avator, username, nickname, description, sex);

          /*
           * emit a signal to attach auth-friend messages to chatting history
           * This is the first offical chatting record,
           * so during this phase, "thread_id" will be dstributed to this
           * chatting thread!
           */
          emit signal_add_auth_friend_init_chatting_thread(chat_type, thread_id,
                                                           namecard, list);
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
          qDebug() << "TEXTCHATMSGRESPONSE failed! Because Of Error Code = "
                   << json["error"].toInt() << '\n';
          return;
        }

        if (!json["verified_msg"].isArray()) {
          qDebug() << "SERVICE_TEXTCHATMSGRESPONSE Json Parse Error!";
          return;
        }

        auto text_sender = json["text_sender"].toString();
        auto text_receiver = json["text_receiver"].toString();
        auto msg_arr = json["verified_msg"].toArray();

        for (const auto &item : msg_arr) {

          if (!item.isObject()) {
            continue;
          }

          auto obj = item.toObject();
          auto thread_id = obj["thread_id"].toString();
          auto unique_id = obj["unique_id"].toString();
          auto msg_id = obj["msg_id"].toString();

          emit signal_update_local2verification_status(thread_id, unique_id,
                                                       msg_id);
        }
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_TEXTCHATMSGICOMINGREQUEST,
      [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "SERVICE_TEXTCHATMSGICOMINGREQUEST Json Parse Error!";

          return;

        } else if (json["error"].toInt() !=
                   static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "SERVICE_TEXTCHATMSGICOMINGREQUEST"
                      "Receive Incoming Text Chat Msg failed! Because Of Error "
                      "Code = "
                   << json["error"].toInt() << '\n';

          return;

        } else {
          if (!json["text_msg"].isArray()) {
            qDebug() << "SERVICE_TEXTCHATMSGICOMINGREQUEST Json Parse Error!";
            return;
          }

          auto text_sender = json["text_sender"].toString();
          auto text_receiver = json["text_receiver"].toString();
          auto msg_arr = json["text_msg"].toArray();

          for (const auto &item : msg_arr) {

            if (!item.isObject()) {
              continue;
            }

            auto obj = item.toObject();

            [[maybe_unused]] auto msg_sender = obj["msg_sender"].toString();
            [[maybe_unused]] auto msg_receiver = obj["msg_receiver"].toString();
            auto thread_id = obj["thread_id"].toString();
            auto unique_id = obj["unique_id"].toString();
            auto msg_id = obj["msg_id"].toString();
            auto msg_content = obj["msg_content"].toString();

            auto data = std::make_shared<ChattingTextMsg>(
                text_sender, text_receiver, unique_id, msg_content);
            data->setMsgID(msg_id);

            emit signal_incoming_msg(MsgType::TEXT, data);
          }
        }
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_HEARTBEAT_RESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "HeartBeat Return value Error!";
          return;
        }
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_PULLCHATTHREADRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Pull Chatting Threads Return Error!";
          return;
        }

        if (!json["threads"].isArray()) {
          qDebug() << "Threads Array Error!";
          return;
        }

        // any more data?
        [[maybe_unused]] auto status = json["is_complete"].toBool();

        // if so, whats the next thread_id we are going to use in next round
        // query!
        [[maybe_unused]] auto next_thread_id =
            json["next_thread_id"].toString();

        auto thread_info = std::move(json["threads"].toArray());

        std::vector<std::unique_ptr<ChatThreadMeta>> lists;
        for (const auto &item : thread_info) {
          if (!item.isObject()) {
            continue;
          }

          auto info = item.toObject();

          auto type = info["type"].toString();

          if (type == "GROUP") {
            auto group_item = std::make_unique<ChatThreadMeta>(
                info["thread_id"].toString().toStdString(),
                UserChatType::GROUP);

            lists.push_back(std::move(group_item));
          } else if (type == "PRIVATE") {
            auto private_item = std::make_unique<ChatThreadMeta>(
                info["thread_id"].toString().toStdString(),
                UserChatType::PRIVATE,
                info["user1_uuid"].toString().toStdString(),
                info["user2_uuid"].toString().toStdString());

            lists.push_back(std::move(private_item));
          }
        }

        emit signal_update_chat_thread(std::make_shared<ChatThreadPageResult>(
            status, next_thread_id, std::move(lists)));
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_PULLCHATRECORDRESPONSE, [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Pull Chatting Threads Return Error!";
          return;
        }
        if (!json["chat_messages"].isArray()) {
          qDebug() << "Threads Array Error!";
          return;
        }

        auto thread_id = json["thread_id"].toString();
        auto next_msg_id = json["next_msg_id"].toString();
        bool is_complete = json["is_complete"].toBool();
        auto msg_arr = std::move(json["chat_messages"].toArray());
        std::vector<std::shared_ptr<ChattingRecordBase>> lists;
        for (const auto &item : msg_arr) {
          if (!item.isObject()) {
            continue;
          }

          auto obj = item.toObject();

          [[maybe_unused]] auto msg_sender = obj["msg_sender"].toString();
          [[maybe_unused]] auto msg_receiver = obj["msg_receiver"].toString();
          auto msg_type = static_cast<MsgType>(obj["msg_type"].toInt());
          [[maybe_unused]] auto thread_id = obj["thread_id"].toString();
          [[maybe_unused]] auto status = obj["status"].toInt();
          auto msg_id = obj["msg_id"].toString();
          auto msg_content = obj["msg_content"].toString();
          // auto timestamp = obj["timestamp"].toString();
          if (msg_type == MsgType::TEXT) {
            auto ptr = std::make_shared<ChattingTextMsg>(
                msg_sender, msg_receiver, msg_content);

            ptr->setMsgID(msg_id);

            lists.push_back(ptr);
          } else if (msg_type == MsgType::IMAGE) {
          }
        }

        emit signal_update_chat_msg(std::make_shared<ChatMsgPageResult>(
            is_complete, thread_id, next_msg_id, std::move(lists)));
      }));

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_CREATENEWPRIVATECHAT_RESPONSE,
      [this](QJsonObject &&json) {
        /*error occured!*/
        if (!json.contains("error")) {
          qDebug() << "Json Parse Error!";
          return;
        }
        if (json["error"].toInt() !=
            static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
          qDebug() << "Create New Private Chat Return Error!";
          return;
        }

        [[maybe_unused]] auto my_uuid = json["my_uuid"].toString();
        [[maybe_unused]] auto friend_uuid = json["friend_uuid"].toString();
        [[maybe_unused]] auto thread_id = json["thread_id"].toString();

        emit signal_create_private_chat(my_uuid, friend_uuid, thread_id);
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

void TCPNetworkConnection::slot_connect2_resources_server() {
  qDebug() << "Connecting to Resources Server"
           << "\nuuid = " << ResourceStorageManager::get_instance()->get_uuid()
           << "\nhost = " << ResourceStorageManager::get_instance()->get_host()
           << "\nport = " << ResourceStorageManager::get_instance()->get_port()
           << '\n';

  m_resources_server_socket.connectToHost(
      ResourceStorageManager::get_instance()->get_host(),
      ResourceStorageManager::get_instance()->get_port().toUShort());
}

void TCPNetworkConnection::slot_terminate_chatting_server(
    const QString &uuid, const QString &token) {
  qDebug() << "Terminate From Chatting Server\n";

  QJsonObject json_obj;
  json_obj["uuid"] = uuid;
  json_obj["token"] = token;

  emit send_buffer(ServiceType::SERVICE_LOGOUTSERVER, std::move(json_obj));
}

void TCPNetworkConnection::slot_terminate_resources_server() {
  qDebug() << "Terminate From Resources Server\n";

  if (m_resources_server_socket.isOpen()) {
    m_resources_server_socket.close();
  }
}

void TCPNetworkConnection::terminate_chatting_server() {
  if (m_chatting_server_socket.isOpen()) {
    m_chatting_server_socket.close();
  }
}

void TCPNetworkConnection::slot_send_message(std::shared_ptr<SendNodeType> data,
                                             TargetServer tar) {
  // Oh, No!!!!!!!!!!!!!!!!!!!!!!!
  // No flush, it might causing buffer full!!!!!!!!!
  if (tar == TargetServer::CHATTINGSERVER) {
    m_chatting_server_socket.write(data->get_buffer());
  } else if (tar == TargetServer::RESOURCESSERVER) {
    m_resources_server_socket.write(data->get_buffer());
  }
}

void TCPNetworkConnection::send_data(SendNodeType &&data, TargetServer tar) {

  // Oh, No!!!!!!!!!!!!!!!!!!!!!!!
  // No flush, it might causing buffer full!!!!!!!!!
  if (tar == TargetServer::CHATTINGSERVER) {
    m_chatting_server_socket.write(data.get_buffer());
  } else if (tar == TargetServer::RESOURCESSERVER) {
    m_resources_server_socket.write(data.get_buffer());
  }
}

void TCPNetworkConnection::send_buffer(ServiceType type, QJsonObject &&obj) {

  QJsonDocument doc(std::move(obj));
  auto byte = doc.toJson(QJsonDocument::Compact);

  /*it should be store as a temporary object, because send_buffer will modify
   * it!*/
  auto buffer = std::make_shared<SendNodeType>(
      static_cast<uint16_t>(type), byte, ByteOrderConverterReverse{});

  /*after connection to server, send TCP request*/
  emit TCPNetworkConnection::get_instance() -> signal_send_message(buffer);
}
