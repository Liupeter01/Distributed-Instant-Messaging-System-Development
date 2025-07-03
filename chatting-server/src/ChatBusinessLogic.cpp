#include <handler/SyncLogic.hpp>
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
