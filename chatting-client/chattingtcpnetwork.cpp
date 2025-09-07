#include "chattingtcpnetwork.h"
#include "useraccountmanager.hpp"

ChattingTCPNetwork::ChattingTCPNetwork()
    : TCPNetworkBase{MsgNodeType::MSGNODE_NORMAL,
                     TargetServer::CHATTINGSERVER} {
  /*register meta type, it should be done BEFORE connect*/
  registerMetaType();

  /*callbacks should be registered at first(before signal)*/
  registerCallback();

  /*register connection event*/
  registerNetworkEvent();
}

void ChattingTCPNetwork::registerNetworkEvent() {}

void ChattingTCPNetwork::registerCallback() {
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

void ChattingTCPNetwork::registerMetaType() {
  qRegisterMetaType<MsgType>("MsgType");
  qRegisterMetaType<ServiceType>("ServiceType");
  qRegisterMetaType<ServiceStatus>("ServiceStatus");
  qRegisterMetaType<UserChatType>("UserChatType");
  qRegisterMetaType<TargetServer>("TargetServer");
  qRegisterMetaType<SendNodeType>("SendNodeType");
  qRegisterMetaType<UserNameCard>("UserNameCard");

  qRegisterMetaType<std::shared_ptr<UserNameCard>>(
      "std::shared_ptr<UserNameCard>");
  qRegisterMetaType<std::shared_ptr<ChatMsgPageResult>>(
      "std::shared_ptr<ChatMsgPageResult>");
  qRegisterMetaType<std::shared_ptr<ChatThreadPageResult>>(
      "std::shared_ptr<ChatThreadPageResult>");
  qRegisterMetaType<std::shared_ptr<ChattingBaseType>>(
      "std::shared_ptr<ChattingBaseType>");
  qRegisterMetaType<std::vector<std::shared_ptr<FriendingConfirmInfo>>>(
      "std::vector<std::shared_ptr<FriendingConfirmInfo>>");
  qRegisterMetaType<std::optional<std::shared_ptr<UserFriendRequest>>>(
      "std::optional<std::shared_ptr<UserFriendRequest>>");
  qRegisterMetaType<std::optional<std::shared_ptr<UserNameCard>>>(
      "std::optional<std::shared_ptr<UserNameCard>>");
  qRegisterMetaType<std::shared_ptr<SendNodeType>>(
      "std::shared_ptr<SendNodeType>");
}

void ChattingTCPNetwork::readyReadHandler(const uint16_t id,
                                          QJsonObject &&obj) {

  try {
    m_callbacks[static_cast<ServiceType>(id)](std::move(obj));
  } catch (const std::exception &e) {
    qDebug() << e.what();
  }
}

void ChattingTCPNetwork::slot_terminate_server() {
  QJsonObject json_obj;
  json_obj["uuid"] = UserAccountManager::get_instance()->get_uuid();
  json_obj["token"] = UserAccountManager::get_instance()->get_token();

  send_buffer(ServiceType::SERVICE_LOGOUTSERVER, std::move(json_obj));
}

void ChattingTCPNetwork::slot_connect2_server() {
  qDebug() << "Connecting to Chatting Server"
           << "\nuuid = " << UserAccountManager::get_instance()->get_uuid()
           << "\nhost = " << UserAccountManager::get_instance()->get_host()
           << "\nport = " << UserAccountManager::get_instance()->get_port()
           << "\ntoken = " << UserAccountManager::get_instance()->get_token()
           << '\n';

  /*the successful or unsuccessful signal is going to generate in
   * signal<->slot*/
  m_socket.connectToHost(
      UserAccountManager::get_instance()->get_host(),
      UserAccountManager::get_instance()->get_port().toUShort());
}
