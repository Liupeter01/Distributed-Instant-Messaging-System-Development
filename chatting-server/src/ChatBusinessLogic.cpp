#include <handler/SyncLogic.hpp>
#include <grpc/GrpcUserService.hpp>
#include <grpc/GrpcDistributedChattingService.hpp>
#include <grpc/GrpcRegisterChattingService.hpp>

void SyncLogic::registerCallbacks() {
          /*
           * ServiceType::SERVICE_LOGINSERVER
           * Handling Login Request
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_LOGINSERVER,
                    std::bind(&SyncLogic::handlingLogin, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));

          /*
           * ServiceType::SERVICE_LOGOUTSERVER
           * Handling Logout Request
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_LOGOUTSERVER,
                    std::bind(&SyncLogic::handlingLogout, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));

          /*
           * ServiceType::SERVICE_SEARCHUSERNAME
           * Handling User Search Username
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_SEARCHUSERNAME,
                    std::bind(&SyncLogic::handlingUserSearch, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));

          /*
           * ServiceType::SERVICE_CREATENEWPRIVATECHAT
           * Handling User Search Username
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_CREATENEWPRIVATECHAT,
                    std::bind(&SyncLogic::handlingCreateNewPrivateChat, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));

          /*
           * ServiceType::SERVICE_CREATENEWPRIVATECHAT
           * Handling User Search Username
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_PULLCHATTHREAD,
                    std::bind(&SyncLogic::handlingUserChatTheads, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));

          /*
           * ServiceType::FRIENDREQUEST_SRC
           * Handling the person who added other(dst) as a friend
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_FRIENDREQUESTSENDER,
                    std::bind(&SyncLogic::handlingFriendRequestCreator, this,
                              std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3)));

          /*
           * ServiceType::FRIENDREQUEST_DST
           * Handling the person was being added response to the person who init this
           * action
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_FRIENDREQUESTCONFIRM,
                    std::bind(&SyncLogic::handlingFriendRequestConfirm, this,
                              std::placeholders::_1, std::placeholders::_2,
                              std::placeholders::_3)));

          /*
           * ServiceType::SERVICE_TEXTCHATMSGREQUEST
           * Handling the user send chatting text msg to others
           */
          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_TEXTCHATMSGREQUEST,
                    std::bind(&SyncLogic::handlingTextChatMsg, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));

          m_callbacks.insert(std::pair<ServiceType, CallbackFunc>(
                    ServiceType::SERVICE_HEARTBEAT_REQUEST,
                    std::bind(&SyncLogic::handlingHeartBeat, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)));
}

void SyncLogic::handlingHeartBeat(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv) {
          boost::json::object src_root;    /*store json from client*/
          boost::json::object result_root; /*send processing result back to dst user*/

          parseJson(session, recv, src_root);

          std::string uuid = boost::json::value_to<std::string>(src_root["uuid"]);

          result_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);

          /*send it back*/
          session->sendMessage(ServiceType::SERVICE_HEARTBEAT_RESPONSE,
                    boost::json::serialize(result_root), session);
}

void SyncLogic::handlingLogin(ServiceType srv_type,
          std::shared_ptr<Session> session, NodePtr recv) {

          RedisRAII raii;

          boost::json::object src_obj;
          boost::json::object redis_root;

          boost::json::array friendreq;  // pending request
          boost::json::array authfriend; // friends that have already been added

          parseJson(session, recv, src_obj);

          // Parsing failed
          if (!(src_obj.contains("uuid") && src_obj.contains("token"))) {
                    generateErrorMessage("Failed to parse json data",
                              ServiceType::SERVICE_LOGINRESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          /*this session has already logined on this server*/
          if (check_and_kick_existing_session(session)) {
                    return;
          }

          std::string uuid = boost::json::value_to<std::string>(src_obj["uuid"]);
          std::string token = boost::json::value_to<std::string>(src_obj["token"]);

          spdlog::info("[{}] UUID = {} Trying to Establish Connection with Token {}",
                    ServerConfig::get_instance()->GrpcServerName, uuid, token);

          auto uuid_value_op = tools::string_to_value<std::size_t>(uuid);
          if (!uuid_value_op.has_value()) {
                    generateErrorMessage("Failed to convert string to number",
                              ServiceType::SERVICE_LOGINRESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          auto response =
                    gRPCGrpcUserService::userLoginToServer(uuid_value_op.value(), token);

          if (response.error() !=
                    static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {

                    spdlog::warn("[{}] UUID = {} Trying to Establish Connection Failed",
                              ServerConfig::get_instance()->GrpcServerName, uuid);

                    generateErrorMessage("Internel Server Error",
                              ServiceType::SERVICE_LOGINRESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          /*update redis cache*/
          updateRedisCache(raii, uuid, session);

          /*bind uuid with a session*/
          session->setUUID(uuid);

          /* add user uuid and session as a pair and store it inside usermanager */
          UserManager::get_instance()->createUserSession(uuid, session);

          /*
           * get user's basic info(name, age, sex, ...) from redis
           * 1. we are going to search for info inside redis first, if nothing found,
           * then goto 2
           * 2. searching for user info inside mysql
           */
          auto info_str = getUserBasicInfo(uuid);
          if (!info_str.has_value()) {

                    spdlog::warn("[{}] UUID = {} Not Located in MySQL & Redis!",
                              ServerConfig::get_instance()->GrpcServerName, uuid);

                    generateErrorMessage("No User Account Found",
                              ServiceType::SERVICE_LOGINRESPONSE,
                              ServiceStatus::LOGIN_INFO_ERROR, session);

                    kick_session(session);
                    return;
          }

          /*returning info to client*/
          std::unique_ptr<user::UserNameCard> info = std::move(*info_str);
          redis_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
          redis_root["uuid"] = uuid;
          redis_root["sex"] = static_cast<uint8_t>(info->m_sex);
          redis_root["avator"] = info->m_avatorPath;
          redis_root["username"] = info->m_username;
          redis_root["nickname"] = info->m_nickname;
          redis_root["description"] = info->m_description;

          /*
           * get friend request list from the database
           * The default startpos = 0, interval = 10
           */
          auto requestlist_op = getFriendRequestInfo(uuid);

          if (requestlist_op.has_value()) {
                    for (auto& req : requestlist_op.value()) {
                              boost::json::object obj;
                              obj["src_uuid"] = req->getNameCard().m_uuid;
                              obj["dst_uuid"] = req->receiver_uuid;
                              obj["username"] = req->getNameCard().m_username;
                              obj["avator"] = req->getNameCard().m_avatorPath;
                              obj["nickname"] = req->getNameCard().m_nickname;
                              obj["description"] = req->getNameCard().m_description;
                              obj["message"] = req->request_message;
                              obj["sex"] = static_cast<uint8_t>(req->getNameCard().m_sex);
                              // redis_root["FriendRequestList"]
                              friendreq.push_back(std::move(obj));
                    }
          }

          /*acquire Friend List*/
          auto friendlist_op = getAuthFriendsInfo(uuid);
          if (friendlist_op.has_value()) {
                    for (auto& req : friendlist_op.value()) {
                              boost::json::object obj;
                              obj["uuid"] = req->m_uuid;
                              obj["username"] = req->m_username;
                              obj["avator"] = req->m_avatorPath;
                              obj["nickname"] = req->m_nickname;
                              obj["description"] = req->m_description;
                              obj["sex"] = static_cast<uint8_t>(req->m_sex);
                              // redis_root["AuthFriendList"].append(obj);
                              authfriend.push_back(std::move(obj));
                    }
          }

          redis_root["error"] = response.error();
          redis_root["FriendRequestList"] = std::move(friendreq);
          redis_root["AuthFriendList"] = std::move(authfriend);

          /*send it back*/
          session->sendMessage(ServiceType::SERVICE_LOGINRESPONSE,
                    boost::json::serialize(redis_root), session);

          /*
           * add user connection counter for current server
           * 1. HGET not exist: Current Chatting server didn't setting up connection
           * counter
           * 2. HGET exist: Increment by 1
           */
          incrementConnection();
}

void SyncLogic::handlingLogout(ServiceType srv_type,
          std::shared_ptr<Session> session, NodePtr recv) {

          RedisRAII raii;

          boost::json::object src_obj;
          boost::json::object result_root; /*send processing result back to src user*/

          parseJson(session, recv, src_obj);

          // Parsing failed
          if (!(src_obj.contains("uuid") && src_obj.contains("token"))) {
                    generateErrorMessage("Failed to parse json data",
                              ServiceType::SERVICE_LOGOUTRESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          std::string uuid = boost::json::value_to<std::string>(src_obj["uuid"]);
          std::string token = boost::json::value_to<std::string>(src_obj["token"]);

          spdlog::info("[{}] UUID {} Trying to Close Connection with Token {}",
                    ServerConfig::get_instance()->GrpcServerName, uuid, token);

          auto uuid_value_op = tools::string_to_value<std::size_t>(uuid);
          if (!uuid_value_op.has_value()) {
                    generateErrorMessage("Failed to convert string to number",
                              ServiceType::SERVICE_LOGOUTRESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          auto response =
                    gRPCGrpcUserService::userLogoutFromServer(uuid_value_op.value(), token);

          if (response.error() !=
                    static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {

                    spdlog::warn("[{}] UUID {} Trying to Logout Failed! Error Code: {}",
                              ServerConfig::get_instance()->GrpcServerName, uuid,
                              response.error());

                    generateErrorMessage("Internel Server Error",
                              ServiceType::SERVICE_LOGOUTRESPONSE,
                              ServiceStatus::LOGOUT_UNSUCCESSFUL, session);
                    return;
          }

          session->sendOfflineMessage();
          UserManager::get_instance()->moveUserToTerminationZone(uuid);
          UserManager::get_instance()->removeUsrSession(uuid,
                    session->get_session_id());

          spdlog::info("[{}] UUID {} Was Removed From Redis Cache And Kick Out Of "
                    "Server Successfully",
                    ServerConfig::get_instance()->GrpcServerName, uuid);
}

void SyncLogic::handlingUserSearch(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv) {

          /*connection pool RAII*/
          MySQLRAII mysql;

          boost::json::object src_obj;  /*store json from client*/
          boost::json::object dst_root; /*store json from client*/

          parseJson(session, recv, src_obj);

          // Parsing failed
          if (!src_obj.contains("username")) {
                    generateErrorMessage("Failed to parse json data",
                              ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          std::string username =
                    boost::json::value_to<std::string>(src_obj["username"]);
          spdlog::info("[{}] User {} Searching For User {} ",
                    ServerConfig::get_instance()->GrpcServerName, session->s_uuid,
                    username);

          /*search username in mysql to get uuid*/
          std::optional<std::size_t> uuid_op =
                    mysql->get()->getUUIDByUsername(username);

          if (!uuid_op.has_value()) {
                    spdlog::warn("[{}] {} Can not find a single user in MySQL and Redis",
                              ServerConfig::get_instance()->GrpcServerName, username);

                    generateErrorMessage("No Username Found In DB",
                              ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                              ServiceStatus::SEARCHING_USERNAME_NOT_FOUND, session);
                    return;
          }

          auto card_op = getUserBasicInfo(std::to_string(uuid_op.value()));

          /*when user info not found!*/
          if (!card_op.has_value()) {
                    spdlog::warn("[{}] No {}'s Profile Found!",
                              ServerConfig::get_instance()->GrpcServerName, uuid_op.value());
                    generateErrorMessage("No User Account Found",
                              ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                              ServiceStatus::SEARCHING_USERNAME_NOT_FOUND, session);
                    return;
          }

          std::unique_ptr<user::UserNameCard> info = std::move(*card_op);
          dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
          dst_root["uuid"] = info->m_uuid;
          dst_root["sex"] = static_cast<uint8_t>(info->m_sex);
          dst_root["avator"] = info->m_avatorPath;
          dst_root["username"] = info->m_username;
          dst_root["nickname"] = info->m_nickname;
          dst_root["description"] = info->m_description;

          session->sendMessage(ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                    boost::json::serialize(dst_root), session);
}

/*the person who init friend request*/
void SyncLogic::handlingFriendRequestCreator(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv) {

          /*connection pool RAII*/
          RedisRAII raii;
          MySQLRAII mysql;

          boost::json::object src_root;    /*store json from client*/
          boost::json::object result_root; /*send processing result back to src user*/
          parseJson(session, recv, src_root);

          // Parsing failed
          if (!(src_root.contains("src_uuid") && src_root.contains("dst_uuid"))) {
                    generateErrorMessage("Failed to parse json data",
                              ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          auto src_uuid =
                    boost::json::value_to<std::string>(src_root["src_uuid"]); // my uuid
          auto dst_uuid =
                    boost::json::value_to<std::string>(src_root["dst_uuid"]); // target uuid
          auto msg = boost::json::value_to<std::string>(src_root["message"]);
          auto nickname = boost::json::value_to<std::string>(src_root["nickname"]);

          if (src_uuid == dst_uuid) {
                    generateErrorMessage("Do Not Add yourself as a friend",
                              ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                              ServiceStatus::FRIENDING_YOURSELF, session);

                    spdlog::warn("[{}]: Receive UUID = {} Friending itself!",
                              ServerConfig::get_instance()->GrpcServerName, src_uuid);
                    return;
          }

          spdlog::info("[{}]: Receive UUID = {}'s Friend Request to UUID = {}",
                    ServerConfig::get_instance()->GrpcServerName, src_uuid,
                    dst_uuid);

          auto src_uuid_value_op = tools::string_to_value<std::size_t>(src_uuid);
          auto dst_uuid_value_op = tools::string_to_value<std::size_t>(dst_uuid);

          if (!src_uuid_value_op.has_value() || !dst_uuid_value_op.has_value()) {
                    spdlog::warn("Casting string typed key to std::size_t!");
                    return;
          }

          /*insert friend request info into mysql db*/
          if (!mysql->get()->createFriendRequest(src_uuid_value_op.value(),
                    dst_uuid_value_op.value(), nickname,
                    msg)) {
                    generateErrorMessage(" Insert Friend Request Failed",
                              ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                              ServiceStatus::FRIENDING_ERROR, session);

                    spdlog::warn("[{} UUID = {}]:  Insert Friend Request Failed",
                              ServerConfig::get_instance()->GrpcServerName, src_uuid);
                    return;
          }

          spdlog::info("[{} UUID = {}]:  Insert Friend Request Successful",
                    ServerConfig::get_instance()->GrpcServerName, src_uuid);

          /*
           * Search For User Belonged Server Cache in Redis
           * find key = server_prefix + dst_uuid in redis, GET
           */
          std::optional<std::string> server_op =
                    raii->get()->checkValue(server_prefix + dst_uuid);

          /*we cannot find it in Redis directly*/
          if (!server_op.has_value()) {
                    generateErrorMessage("User Not Found In Any Server",
                              ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                              ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                              session);
                    return;
          }

          /*We have to get src_uuid info on current server */
          auto info_str = getUserBasicInfo(src_uuid);
          if (!info_str.has_value()) {
                    generateErrorMessage("Current UserProfile Load Error!",
                              ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                              ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                              session);
                    return;
          }

          std::unique_ptr<user::UserNameCard> src_namecard = std::move(*info_str);

          /*Is target user(dst_uuid) and current user(src_uuid) on the same server*/
          if (server_op.value() == ServerConfig::get_instance()->GrpcServerName) {
                    /*try to find this target user on current chatting-server*/
                    auto session_op = UserManager::get_instance()->getSession(dst_uuid);
                    if (!session_op.has_value()) {
                              generateErrorMessage("Target User's Session Not Found",
                                        ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                                        ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                                        session);
                              return;
                    }
                    /*send it to dst user*/
                    boost::json::object dst_root;

                    dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
                    dst_root["src_uuid"] = src_uuid;
                    dst_root["dst_uuid"] = dst_uuid;
                    dst_root["src_nickname"] = nickname;
                    dst_root["src_message"] = msg;
                    dst_root["src_avator"] = src_namecard->m_avatorPath;
                    dst_root["src_username"] = src_namecard->m_username;
                    dst_root["src_desc"] = src_namecard->m_description;
                    dst_root["src_sex"] = static_cast<uint8_t>(src_namecard->m_sex);

                    auto session_ptr = session_op.value();

                    /*propagate the message to dst user*/
                    session_ptr->sendMessage(ServiceType::SERVICE_FRIENDREINCOMINGREQUEST,
                              boost::json::serialize(dst_root), session_ptr);

                    result_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
          }
          else {
                    /*
                     * GRPC REQUEST
                     * dst_uuid and src_uuid are not on the same server-
                     * Pass current user info to other chatting-server
                     * by using grpc protocol
                     */
                    message::FriendRequest grpc_request;
                    grpc_request.set_src_uuid(src_uuid_value_op.value());
                    grpc_request.set_dst_uuid(dst_uuid_value_op.value());
                    grpc_request.set_nick_name(nickname);
                    grpc_request.set_req_msg(msg);
                    grpc_request.set_avator_path(src_namecard->m_avatorPath);
                    grpc_request.set_username(src_namecard->m_username);
                    grpc_request.set_description(src_namecard->m_description);
                    grpc_request.set_sex(static_cast<uint8_t>(src_namecard->m_sex));

                    auto response =
                              gRPCDistributedChattingService::get_instance()->sendFriendRequest(
                                        server_op.value(), grpc_request);
                    if (response.error() !=
                              static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
                              spdlog::warn("[GRPC {} Service]: UUID = {} Send Request To GRPC {} "
                                        "Service Failed!",
                                        ServerConfig::get_instance()->GrpcServerName, src_uuid,
                                        server_op.value());
                              generateErrorMessage("Internel Server Error",
                                        ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                                        ServiceStatus::FRIENDING_ERROR, session);
                              return;
                    }

                    result_root["error"] = response.error();
          }

          /*send service result back to request sender*/
          result_root["src_uuid"] = src_uuid;
          result_root["dst_uuid"] = dst_uuid;
          session->sendMessage(ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                    boost::json::serialize(result_root), session);
}

void SyncLogic::handlingUserChatTheads(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv) {

          MySQLRAII mysql;
          boost::json::object src_obj;
          boost::json::object result_obj;

          parseJson(session, recv, src_obj);

          // Parsing failed
          if (!(src_obj.contains("uuid") && src_obj.contains("thread_id"))) {
                    generateErrorMessage("Failed to parse json data",
                              ServiceType::SERVICE_PULLCHATTHREADRESPONSE,
                              ServiceStatus::JSONPARSE_ERROR, session);
                    return;
          }

          auto uuid = boost::json::value_to<std::string>(src_obj["uuid"]);
          auto thread_id = boost::json::value_to<std::string>(src_obj["thread_id"]);

          if (!tools::string_to_value<std::size_t>(uuid).has_value() ||
                    !tools::string_to_value<std::size_t>(thread_id).has_value()) {
                    generateErrorMessage("Failed to cast uuid strings to size_t",
                              ServiceType::SERVICE_PULLCHATTHREADRESPONSE,
                              ServiceStatus::JSONPARSE_ERROR, session);
                    return;
          }

          bool is_complete{};           //is thread_id list acquire finished!
          std::string next_thread_id;   //next_thread_id order is going to be acquired!

          auto list_status = mysql->get()->getUserChattingThreadIdx(
                    std::stoi(uuid),
                    std::stoi(thread_id),
                    /*interval*/10,
                    next_thread_id,
                    is_complete
          );

          if (!list_status.has_value()) {
                    spdlog::warn("[{}]: Failed To Acquire UUID: [{}] Thread ID Data",
                              ServerConfig::get_instance()->GrpcServerName, uuid);

                    generateErrorMessage("Failed to cast uuid strings to size_t",
                              ServiceType::SERVICE_PULLCHATTHREADRESPONSE,
                              ServiceStatus::CHATTHREAD_PARSE_ERROR, session);
                    return;
          }

          /*Start To append all thread data to json*/
          boost::json::array threads_arr;
          std::size_t size = (*list_status).size();

          for (size_t i = 0; i < size; ++i) {
                    std::unique_ptr<chat::ChatThreadMeta> item = std::move((*list_status)[i]);

                    boost::json::object info;
                    info["thread_id"] = item->_thread_id;
                    info["type"] = (item->_chat_type == chat::UserChatType::GROUP) ? "GROUP" : "PRIVATE";

                    if (!item->isGroupChat()) {
                              info["user1_uuid"] = *(item->_user_one);
                              info["user2_uuid"] = *(item->_user_two);
                    }

                    threads_arr.push_back(info);
          }

          result_obj["is_complete"] = is_complete;
          result_obj["next_thread_id"] = next_thread_id;
          result_obj["threads"] = threads_arr;
          result_obj["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
          session->sendMessage(ServiceType::SERVICE_PULLCHATTHREADRESPONSE,
                    boost::json::serialize(result_obj), session);
}

void SyncLogic::handlingCreateNewPrivateChat(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv)
{
          MySQLRAII mysql;
          boost::json::object src_root;    /*store json from client*/
          boost::json::object result_root; /*send processing result back to src user*/

          parseJson(session, recv, src_root);

          // Parsing failed
          if (!(src_root.contains("my_uuid") && src_root.contains("friend_uuid"))) {
                    generateErrorMessage("Failed to parse json data",
                              ServiceType::SERVICE_CREATENEWPRIVATECHAT_RESPONSE,
                              ServiceStatus::JSONPARSE_ERROR, session);
                    return;
          }

          auto my_uuid = boost::json::value_to<std::string>(src_root["my_uuid"]);
          auto friend_uuid = boost::json::value_to<std::string>(src_root["friend_uuid"]);

          if (!tools::string_to_value<std::size_t>(my_uuid).has_value() ||
                    !tools::string_to_value<std::size_t>(friend_uuid).has_value()) {
                    generateErrorMessage("Failed to cast uuid strings to size_t",
                              ServiceType::SERVICE_CREATENEWPRIVATECHAT_RESPONSE,
                              ServiceStatus::JSONPARSE_ERROR, session);
                    return;
          }

          auto status = mysql->get()->createNewPrivateChat(
                    std::stoi(my_uuid),
                    std::stoi(friend_uuid)
          );

          if (!status.has_value()) {
                    spdlog::warn("[{}]: Failed To Generate Private Chat Thread ID Between UUID: [{}]<->UUID:[{}]!",
                              ServerConfig::get_instance()->GrpcServerName, my_uuid, friend_uuid);

                    generateErrorMessage("Failed To Generate Private Chat Thread ID",
                              ServiceType::SERVICE_CREATENEWPRIVATECHAT_RESPONSE,
                              ServiceStatus::CREATE_PRIVATE_CHAT_FAILED, session);
                    return;
          }

          result_root["my_uuid"] = my_uuid;
          result_root["friend_uuid"] = friend_uuid;
          result_root["thread_id"] = *status;
          result_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
          session->sendMessage(ServiceType::SERVICE_CREATENEWPRIVATECHAT_RESPONSE,
                    boost::json::serialize(result_root), session);
}

/*the person who receive friend request are going to confirm it*/
void SyncLogic::handlingFriendRequestConfirm(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv) {

          auto generate_json = [](const std::string& thread_id,
                    const std::string& friend_uuid,
                    const boost::json::array& arr,
                    std::unique_ptr< user::UserNameCard> card)->boost::json::object
                    {
                              std::unique_ptr<user::UserNameCard> namecard = std::move(card);
                              boost::json::object json;
                              json["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
                              json["friend_uuid"] = friend_uuid;
                              json["friend_username"] = namecard->m_username;
                              json["friend_nickname"] = namecard->m_nickname;
                              json["friend_avator"] = namecard->m_avatorPath;
                              json["friend_desc"] = namecard->m_description;
                              json["friend_sex"] = static_cast<uint8_t>(namecard->m_sex);
                              json["thread_id"] = thread_id;
                              json["chat_type"] = "PRIVATE";
                              json["hello_msg"] = arr;
                              return json;
                    };

          /*connection pool RAII*/
          RedisRAII raii;
          MySQLRAII mysql;

          boost::json::object src_root;    /*store json from client*/
          boost::json::object result_root; /*send processing result back to src user*/

          std::string thread_id;

          parseJson(session, recv, src_root);

          if (!(src_root.contains("src_uuid") &&
                    src_root.contains("dst_uuid"))) {
                    generateErrorMessage("Missing required keys: src_uuid or dst_uuid",
                              ServiceType::SERVICE_SEARCHUSERNAMERESPONSE,
                              ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                    return;
          }

          auto src_uuid = boost::json::value_to<std::string>(src_root["src_uuid"]);     //target user's uuid
          auto dst_uuid = boost::json::value_to<std::string>(src_root["dst_uuid"]);     //confimer's uuid
          auto alternative = boost::json::value_to<std::string>(src_root["alternative_name"]);

          if (!tools::string_to_value<std::size_t>(src_uuid).has_value()
                    || !tools::string_to_value<std::size_t>(dst_uuid).has_value()) {

                    generateErrorMessage("Failed to cast uuid strings to size_t",
                              ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                              ServiceStatus::FRIENDING_ERROR, session);
                    return;
          }

          auto status = mysql->get()->execFriendConfirmationTransaction(
                    std::stoi(src_uuid),
                    std::stoi(dst_uuid)
          );

          if (!status.has_value()) {
                    spdlog::warn("[{}]: Something Goes Wrong Inside Transaction!",
                              ServerConfig::get_instance()->GrpcServerName);

                    generateErrorMessage("Failed to update friending status",
                              ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                              ServiceStatus::FRIENDING_ERROR, session);

                    generateErrorMessage(
                              fmt::format("Failed to create friend relation: {} <-> {}", dst_uuid,
                                        src_uuid),
                              ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                              ServiceStatus::FRIENDING_ERROR, session);

                    return;
          }

          /*
         * Response SERVICE_SUCCESS to the authenticator
         * Current session should receive a successful response first
         */
          result_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
          session->sendMessage(ServiceType::SERVICE_FRIENDCONFIRMRESPONSE,
                    boost::json::serialize(result_root), session);

          spdlog::info("[{}]: Friend confirmed: {} -> {}",
                    ServerConfig::get_instance()->GrpcServerName, src_uuid,
                    dst_uuid);

          spdlog::info("[{}] Bidirectional friend relations created: {} <-> {}",
                    ServerConfig::get_instance()->GrpcServerName, src_uuid,
                    dst_uuid);

          /*We have to get src user info(src_uuid) on current server */
          auto src_info = getUserBasicInfo(src_uuid);
          if (!src_info) {
                    generateErrorMessage("User profile not found (src)",
                              ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                              ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                              session);
                    return;
          }

          boost::json::array message_arr;
          message::AuthoriseRequest grpc_request;
          for (auto& item : *status) {
                    boost::json::object obj;

                    thread_id = item->thread_id;

                    message::AuthoriseMessage* message = grpc_request.add_msg_pack();

                    message->set_msg_type(static_cast<int32_t>(item->message_type));
                    message->set_msg_id(item->message_id);
                    message->set_msg_sender(item->message_sender);
                    message->set_msg_receiver(item->message_receiver);
                    message->set_hello_msg(item->message_content);

                    obj["msg_id"] = item->message_id;
                    obj["msg_sender"] = item->message_sender;
                    obj["msg_receiver"] = item->message_receiver;
                    obj["msg_content"] = item->message_content;
                    obj["msg_type"] = static_cast<int32_t>(item->message_type);
                    message_arr.push_back(std::move(obj));
          }

          // Notify current user (authenticator) with friend's profile
          auto json = generate_json(thread_id, src_uuid, message_arr, std::move(*src_info));
          session->sendMessage(ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                    boost::json::serialize(json), session);

          /*
           * Search For User Belonged Server Cache in Redis
           * find key = server_prefix + src_uuid in redis, GET
           */
          auto server_op = raii->get()->checkValue(server_prefix + src_uuid);

          /*we cannot find it in Redis directly*/
          if (!server_op.has_value()) {
                    spdlog::warn("[{}] : Could Not Find Current User {} In Any Server!",
                              ServerConfig::get_instance()->GrpcServerName, src_uuid);
                    return;
          }

          /*We have to get dst user info(dst_uuid) on current server */
          auto  dst_info = getUserBasicInfo(dst_uuid);

          /*Is target user(src_uuid) and current user(dst_uuid) on the same server*/
          if (server_op.value() == ServerConfig::get_instance()->GrpcServerName) {
                    /*try to find this target user on current chatting-server*/
                    auto session_op = UserManager::get_instance()->getSession(src_uuid);
                    if (!session_op.has_value()) {
                              generateErrorMessage("Invalid Session",
                                        ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                                        ServiceStatus::FRIENDING_ERROR,
                                        *session_op);
                              return;
                    }

                    if (!dst_info.has_value()) {
                              generateErrorMessage("User profile not found (dst)",
                                        ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                                        ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                                        *session_op);
                              return;
                    }

                    boost::json::object root = generate_json(thread_id, dst_uuid, message_arr, std::move(*dst_info));
                    /*propagate the message to dst user*/
                    (*session_op)->sendMessage(ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
                              boost::json::serialize(root), *session_op);

                    return;

          }
          else {

                    /*
                     * GRPC REQUEST
                     * dst_uuid and src_uuid are not on the same server
                     * Pass current user info to other chatting-server
                     * by using grpc protocol
                     */

                    if (!dst_info.has_value()) {
                              return;
                    }

                    std::unique_ptr<user::UserNameCard> namecard = std::move(*dst_info);

                    message::FriendRequest* user_info = grpc_request.add_user_info();
                    user_info->set_dst_uuid(std::stoi(dst_uuid));
                    user_info->set_src_uuid(std::stoi(src_uuid));
                    user_info->set_avator_path(namecard->m_avatorPath);
                    user_info->set_description(namecard->m_description);
                    user_info->set_nick_name(namecard->m_nickname);
                    user_info->set_username(namecard->m_username);
                    user_info->set_sex(static_cast<uint8_t>(namecard->m_sex));

                    grpc_request.set_thread_id(thread_id);
                    grpc_request.set_src_uuid(src_uuid);
                    grpc_request.set_dst_uuid(dst_uuid);

                    auto response =
                              gRPCDistributedChattingService::get_instance()->confirmFriendRequest(
                                        *server_op, grpc_request);

                    if (response.error() !=
                              static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
                              spdlog::warn("[GRPC {} Service]: UUID = {} Send Request To GRPC {} "
                                        "Service Failed!",
                                        ServerConfig::get_instance()->GrpcServerName, src_uuid,
                                        *server_op);
                              return;
                    }

                    spdlog::info("[GRPC {} Service]: UUID = {} Send Request To GRPC {} "
                              "Service Successful!",
                              ServerConfig::get_instance()->GrpcServerName, src_uuid,
                              *server_op);
          }
}

/*Handling the user send chatting text msg to others*/
void SyncLogic::handlingTextChatMsg(ServiceType srv_type,
          std::shared_ptr<Session> session,
          NodePtr recv) {

          /*connection pool RAII*/
          RedisRAII raii;
          MySQLRAII mysql;

          boost::json::object src_root; /*store json from client*/

          parseJson(session, recv, src_root);

          IN OUT std::vector<std::shared_ptr<chat::TextMsgInfo>> updated_msg;

          // Parsing failed
          if (!(src_root.contains("text_sender") &&
                    src_root.contains("text_receiver") &&
                    src_root.contains("text_msg") &&
                    src_root.contains("thread_id"))) {
                    generateErrorMessage("Missing required fields",
                              ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                              ServiceStatus::JSONPARSE_ERROR, session);
                    return;
          }

          auto thread_id =
                    boost::json::value_to<std::string>(src_root["thread_id"]);
          auto sender_uuid =
                    boost::json::value_to<std::string>(src_root["text_sender"]);
          auto receiver_uuid =
                    boost::json::value_to<std::string>(src_root["text_receiver"]);

          const auto msg_array = src_root["text_msg"];

          if (!tools::string_to_value<std::size_t>(sender_uuid).has_value() ||
                    !tools::string_to_value<std::size_t>(receiver_uuid).has_value()) {

                    generateErrorMessage("Invalid UUID format",
                              ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                              ServiceStatus::JSONPARSE_ERROR, session);
                    return;
          }

          //RAII
          {
                    if (!src_root["text_msg"].is_array()) {
                              generateErrorMessage("Missing required fields",
                                        ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                                        ServiceStatus::LOGIN_UNSUCCESSFUL, session);
                              return;
                    }

                    auto arr = src_root["text_msg"].as_array();
                    for (auto& item : arr) {
                              if (!item.is_object())  continue;

                              auto obj = item.as_object();

                              updated_msg.push_back(std::make_shared< chat::TextMsgInfo>(thread_id,
                                        boost::json::value_to<std::string>(obj["unique_id"]),
                                        boost::json::value_to<std::string>(obj["msg_sender"]),
                                        boost::json::value_to<std::string>(obj["msg_receiver"]),
                                        boost::json::value_to<std::string>(obj["msg_content"])
                              ));
                    }
          }

          if (!mysql->get()->createModifyChattingHistoryRecord(updated_msg)) {
                    generateErrorMessage("DataBase Operation Failed!",
                              ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                              ServiceStatus::MYSQL_INTERNAL_ERROR, session);
                    return;
          }

          // Query which server the receiver belongs to
          auto server_op = raii->get()->checkValue(server_prefix + receiver_uuid);
          if (!server_op) {
                    generateErrorMessage(fmt::format("[{}] Cannot Find Receiver {}'s Server Info In  Redis",
                              ServerConfig::get_instance()->GrpcServerName, receiver_uuid),
                              ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                              ServiceStatus::REDIS_UNKOWN_ERROR, session);
                    return;
          }

          // Inner-server
          boost::json::array updated_arr;

          //Unique_id <-> msg_id mapping relation
          boost::json::array mapping_arr;

          // Cross-server: use gRPC to forward
          message::ChattingTextMsgRequest grpc_req;
          grpc_req.set_src_uuid(sender_uuid);
          grpc_req.set_dst_uuid(receiver_uuid);

          for (auto& item : updated_msg) {

                    if (!item->isVerified) {
                              spdlog::error("[{} ]: UUID = {} Send Request To UUID = {} Error, Unexpected Error!",
                                        ServerConfig::get_instance()->GrpcServerName, sender_uuid, receiver_uuid);
                              continue;
                    }

                    boost::json::object obj;
                    boost::json::object mapping;

                    message::ChattingHistoryData* data_item = grpc_req.add_lists();
                    data_item->set_msg_id(item->message_id);
                    data_item->set_thread_id(item->thread_id);
                    data_item->set_unique_id(item->unique_id);
                    data_item->set_msg_sender(item->msg_sender);
                    data_item->set_msg_receiver(item->msg_receiver);
                    data_item->set_msg_content(item->msg_content);

                    obj["thread_id"] = thread_id;
                    obj["unique_id"] = item->unique_id;
                    obj["msg_id"] = item->message_id;
                    obj["msg_sender"] = item->msg_sender;
                    obj["msg_receiver"] = item->msg_receiver;
                    obj["msg_content"] = item->msg_content;

                    mapping["thread_id"] = thread_id;
                    mapping["unique_id"] = item->unique_id;
                    mapping["msg_id"] = item->message_id;

                    updated_arr.push_back(std::move(obj));
                    mapping_arr.push_back(std::move(mapping));
          }

          /*
         * Response SERVICE_SUCCESS to the text msg sender
         * Current session should receive a successful response first
         */
          boost::json::object result_root; // reponse status to sender
          result_root["error"] = static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS);
          result_root["text_sender"] = sender_uuid;
          result_root["text_receiver"] = receiver_uuid;
          result_root["verified_msg"] = mapping_arr;

          session->sendMessage(ServiceType::SERVICE_TEXTCHATMSGRESPONSE,
                    boost::json::serialize(result_root), session);

          /*Is target user and msg text sender on the same server*/
          if (server_op.value() == ServerConfig::get_instance()->GrpcServerName) {

                    /*try to find this target user on current chatting-server*/
                    auto receiver_session =
                              UserManager::get_instance()->getSession(receiver_uuid);
                    if (!receiver_session.has_value()) {
                              generateErrorMessage("Receiver Session Not Found",
                                        ServiceType::SERVICE_FRIENDSENDERRESPONSE,
                                        ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND,
                                        session);
                              return;
                    }

                    /*try to do message forwarding to dst target user*/
                    boost::json::object dst_root;
                    dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
                    dst_root["text_sender"] = sender_uuid;
                    dst_root["text_receiver"] = receiver_uuid;
                    dst_root["text_msg"] = updated_arr;

                    /*propagate the message to dst user*/
                    (*receiver_session)->sendMessage(
                              ServiceType::SERVICE_TEXTCHATMSGICOMINGREQUEST,
                              boost::json::serialize(dst_root), *receiver_session);

                    return;

          }

          message::ChattingTextMsgResponse response =
                    gRPCDistributedChattingService::get_instance()->sendChattingTextMsg(
                              *server_op, grpc_req);

          if (response.error() !=
                    static_cast<std::size_t>(ServiceStatus::SERVICE_SUCCESS)) {
                    spdlog::warn(
                              "[gRPC {}]: Failed to forward message from {} to {} (server: {})",
                              ServerConfig::get_instance()->GrpcServerName, sender_uuid,
                              receiver_uuid, *server_op);
                    return;
          }

          spdlog::info(
                    "[gRPC {}]: Forward message from {} to {} (server: {}) Successful",
                    ServerConfig::get_instance()->GrpcServerName, sender_uuid,
                    receiver_uuid, *server_op);

}