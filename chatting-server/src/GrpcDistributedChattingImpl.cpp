#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <config/ServerConfig.hpp>
#include <grpc/GrpcDistributedChattingImpl.hpp>
#include <handler/SyncLogic.hpp>
#include <server/Session.hpp>
#include <server/UserManager.hpp>
#include <server/UserNameCard.hpp>
#include <spdlog/spdlog.h>

grpc::GrpcDistributedChattingImpl::GrpcDistributedChattingImpl() {}

grpc::GrpcDistributedChattingImpl::~GrpcDistributedChattingImpl() {}

// if another user has already logined on other server, then force it to quit!
::grpc::Status
grpc::GrpcDistributedChattingImpl::ForceTerminateLoginedUser(::grpc::ServerContext* context,
          const ::message::TerminationRequest* request,
          ::message::TerminationResponse* response) {

          auto uuid_str = std::to_string(request->kick_uuid());
          response->set_kick_uuid(request->kick_uuid());
          response->set_error(static_cast<int32_t>(ServiceStatus::SERVICE_SUCCESS));

          if (auto opt = UserManager::get_instance()->getSession(uuid_str); opt) {
                    auto& session = *opt;
                    session->sendOfflineMessage();
                    session->terminateAndRemoveFromServer(uuid_str, session->get_session_id());
          }

          return grpc::Status::OK;
}

// A send friend request message to another user B
::grpc::Status grpc::GrpcDistributedChattingImpl::SendFriendRequest(
    ::grpc::ServerContext *context, const ::message::FriendRequest *request,
    ::message::FriendResponse *response) {
  /*
   * try to locate target user id in this server's user management mapping ds
   * Maybe we can not find this user in the server
   */
  std::optional<std::shared_ptr<Session>> session_op =
      UserManager::get_instance()->getSession(
          std::to_string(request->dst_uuid()));
  if (!session_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: Find Target User {} Error!",
                 ServerConfig::get_instance()->GrpcServerName,
                 request->dst_uuid());
    response->set_error(
        static_cast<uint8_t>(ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND));
  } else {
    /*setup json data and forward to target user*/
    boost::json::object root;
    root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    root["src_uuid"] = std::to_string(request->src_uuid());
    root["dst_uuid"] = std::to_string(request->dst_uuid());
    root["src_nickname"] = request->nick_name();
    root["src_message"] = request->req_msg();
    root["src_avator"] = request->avator_path();
    root["src_username"] = request->username();
    root["src_desc"] = request->description();
    root["src_sex"] = request->sex();

    /*send a forwarding packet*/
    session_op.value()->sendMessage(
        ServiceType::SERVICE_FRIENDREINCOMINGREQUEST,
        boost::json::serialize(root));

    /*setup response*/
    response->set_src_uuid(request->src_uuid());
    response->set_dst_uuid(request->dst_uuid());
    response->set_error(static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS));
  }

  return grpc::Status::OK;
}

// User B agreed with user A's friend adding request
::grpc::Status grpc::GrpcDistributedChattingImpl::ConfirmFriendRequest(
    ::grpc::ServerContext *context, const ::message::FriendRequest *request,
    ::message::FriendResponse *response) {

  /*
   * try to locate target user id in this server's user management mapping ds
   * Maybe we can not find this user in the server
   */
  std::optional<std::shared_ptr<Session>> session_op =
      UserManager::get_instance()->getSession(
          std::to_string(request->src_uuid()));
  if (!session_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: Find Target User {} Error!",
                 ServerConfig::get_instance()->GrpcServerName,
                 request->src_uuid());
    response->set_error(
        static_cast<uint8_t>(ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND));
  } else {
    /*setup json data and forward to target user*/
    boost::json::object root;

    root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    root["friend_uuid"] = request->dst_uuid();
    root["friend_nickname"] = request->nick_name();
    root["friend_avator"] = request->avator_path();
    root["friend_username"] = request->username();
    root["friend_desc"] = request->description();
    root["friend_sex"] = request->sex();

    /*send a forwarding packet*/
    session_op.value()->sendMessage(
        ServiceType::SERVICE_FRIENDING_ON_BIDDIRECTIONAL,
        boost::json::serialize(root));

    /*setup response*/
    response->set_src_uuid(request->src_uuid());
    response->set_dst_uuid(request->dst_uuid());
    response->set_error(static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS));
  }

  return grpc::Status::OK;
}

// Verify that B is still A's friend:
::grpc::Status grpc::GrpcDistributedChattingImpl::FriendshipVerification(
    ::grpc::ServerContext *context, const ::message::AuthoriseRequest *request,
    ::message::AuthoriseResponse *response) {

  return grpc::Status::OK;
}

// transfer chatting message from user A to B
::grpc::Status grpc::GrpcDistributedChattingImpl::SendChattingTextMsg(
    ::grpc::ClientContext *context,
    const ::message::ChattingTextMsgRequest *request,
    ::message::ChattingTextMsgResponse *response) {

  /*
   * try to locate target user id in this server's user management mapping ds
   * Maybe we can not find this user in the server
   */
  std::optional<std::shared_ptr<Session>> session_op =
      UserManager::get_instance()->getSession(
          std::to_string(request->dst_uuid()));

  if (!session_op.has_value()) {
    spdlog::warn("[GRPC {} Service]: Find Target User {} Error!",
                 ServerConfig::get_instance()->GrpcServerName,
                 request->src_uuid());
    response->set_error(
        static_cast<uint8_t>(ServiceStatus::FRIENDING_TARGET_USER_NOT_FOUND));
  } else {
    boost::json::array msg_array; /*try to parse grpc repeated array*/
    boost::json::object
        dst_root; /*try to do message forwarding to dst target user*/

    /*get server lists*/
    auto &msg_lists = request->lists();

    /*traversal server lists and create multiple DistributedChattingServicePool
     * according to host and port*/
    std::for_each(msg_lists.begin(), msg_lists.end(),
                  [&msg_array](decltype(*msg_lists.begin()) &server) {
                    boost::json::object msg;

                    /*msg sender and msg receiver identity*/
                    msg["msg_sender"] = server.msg_sender();
                    msg["msg_receiver"] = server.msg_receiver();

                    /*generate an unique uuid for this message*/
                    msg["msg_id"] = server.msg_id();

                    /*send message*/
                    msg["msg_content"] = server.msg_content();
                    msg_array.push_back(std::move(msg));
                  });

    dst_root["error"] = static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS);
    dst_root["text_sender"] = request->src_uuid();
    dst_root["text_receiver"] = request->dst_uuid();
    dst_root["text_msg"] = std::move(msg_array);

    /*send a forwarding packet*/
    session_op.value()->sendMessage(
        ServiceType::SERVICE_TEXTCHATMSGICOMINGREQUEST,
        boost::json::serialize(dst_root));

    /*setup response*/
    response->set_src_uuid(request->src_uuid());
    response->set_dst_uuid(request->dst_uuid());
    response->set_error(static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS));
  }
  return grpc::Status::OK;
}
