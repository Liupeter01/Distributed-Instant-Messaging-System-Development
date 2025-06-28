#ifndef _CHATTINGTHREADDEF_H
#define _CHATTINGTHREADDEF_H
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chat {
enum class UserChatType { PRIVATE, GROUP };

/*
 * ThreadInfo is a TEMP structure aiming to store the the relationship of
 * thread_id <--Private Chat-> (user_one, user_two)
 * thread_id <--Group Chat-> (None)
 */
struct ChatThreadMeta {
  ChatThreadMeta() = default;
  ChatThreadMeta(const std::string &__thread_id, const UserChatType _type,
                 std::optional<std::string> one = std::nullopt,
                 std::optional<std::string> two = std::nullopt)
      : _thread_id(__thread_id), _chat_type(_type), _user_one(one),
        _user_two(two) {}

  bool isGroupChat() const { return _chat_type == UserChatType::GROUP; }

  static ChatThreadMeta createPrivateChat(const std::string &thread_id,
                                          const std::string &user_one,
                                          const std::string &user_two) {
    return ChatThreadMeta(thread_id, UserChatType::PRIVATE, user_one, user_two);
  }

  static ChatThreadMeta createGroupChat(const std::string &thread_id) {
    return ChatThreadMeta(thread_id, UserChatType::GROUP);
  }

  std::string _thread_id;
  UserChatType _chat_type;

  // those two variables are for private chat, if its a group chat then ignore
  // them
  std::optional<std::string> _user_one = std::nullopt;
  std::optional<std::string> _user_two = std::nullopt;
};

struct ChattingThreadDesc {

  ChattingThreadDesc() = default;
  ChattingThreadDesc(const ChatThreadMeta &o)
      : _meta(o._thread_id, o._chat_type, o._user_one, o._user_two),
        _last_msg_id(std::nullopt) {}

  ChattingThreadDesc(const std::string &__thread_id, const UserChatType _type,
                     std::optional<std::string> __last_msg_id = std::nullopt,
                     std::optional<std::string> one = std::nullopt,
                     std::optional<std::string> two = std::nullopt)
      : _meta(__thread_id, _type, one, two), _last_msg_id(__last_msg_id) {}

  bool isGroupChat() const { return _meta.isGroupChat(); }
  const std::string &getThreadId() const { return _meta._thread_id; }
  std::optional<std::string> getLastMsgId() const { return _last_msg_id; }
  std::optional<std::string> getUserOneUUID() const { return _meta._user_one; }
  std::optional<std::string> getUserTwoUUID() const { return _meta._user_two; }

  void setLastMsgId(const std::string &last) { _last_msg_id = last; }

  static ChattingThreadDesc createPrivateChat(const std::string &thread_id,
                                              const std::string &user_one,
                                              const std::string &user_two) {
    return ChattingThreadDesc(thread_id, UserChatType::PRIVATE, std::nullopt,
                              user_one, user_two);
  }

  static ChattingThreadDesc createGroupChat(const std::string &thread_id) {
    return ChattingThreadDesc(thread_id, UserChatType::GROUP);
  }
  ChatThreadMeta _meta;
  std::optional<std::string> _last_msg_id; // last msg id for data retrieve!
};
} // namespace chat

#endif //_CHATTINGTHREADDEF_H
