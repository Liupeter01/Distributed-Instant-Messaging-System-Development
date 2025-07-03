#pragma once
#ifndef _GRPCDISRIBUTEDCHATTINGIMPL_
#define _GRPCDISRIBUTEDCHATTINGIMPL_
#include <grpcpp/grpcpp.h>
#include <message/message.grpc.pb.h>
#include <mutex>
#include <network/def.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>

#define ALLOW_INFO_PACKS 1
#define ALLOW_MSG_PACKS 2

namespace grpc {
class GrpcDistributedChattingImpl final
    : public message::DistributedChattingService::Service {

public:
  GrpcDistributedChattingImpl();
  virtual ~GrpcDistributedChattingImpl();

public:
  // if another user has already logined on other server, then force it to quit!
  virtual ::grpc::Status
  ForceTerminateLoginedUser(::grpc::ServerContext *context,
                            const ::message::TerminationRequest *request,
                            ::message::TerminationResponse *response);

  // A send friend request message to another user B
  virtual ::grpc::Status
  SendFriendRequest(::grpc::ServerContext *context,
                    const ::message::FriendRequest *request,
                    ::message::FriendResponse *response);

  // User B agreed with user A's friend adding request
  virtual ::grpc::Status
  ConfirmFriendRequest(::grpc::ServerContext* context, const ::message::AuthoriseRequest* request,
            ::message::AuthoriseResponse* response);

  // transfer chatting message from user A to B
  virtual ::grpc::Status
  SendChattingTextMsg(::grpc::ClientContext *context,
                      const ::message::ChattingTextMsgRequest *request,
                      ::message::ChattingTextMsgResponse *response);

private:
};
} // namespace grpc

#endif //_GRPCDISRIBUTEDCHATTINGIMPL_
