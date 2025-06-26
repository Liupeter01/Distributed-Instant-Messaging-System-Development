#ifndef _CHATTINGTHREADDEF_H
#define _CHATTINGTHREADDEF_H
#include <vector>
#include <memory>
#include <iostream>
#include <optional>
#include <string>

namespace chat {
          enum class UserChatType {
                    PRIVATE, GROUP
          };

          /*
           * ThreadInfo is a TEMP structure aiming to store the the relationship of
           * thread_id <--Private Chat-> (user_one, user_two)
           * thread_id <--Group Chat-> (None)
           */
          struct ChatThreadMeta {
                    ChatThreadMeta() = default;
                    ChatThreadMeta(const std::string& __thread_id,
                              const UserChatType _type,
                              std::optional<std::string> one = std::nullopt,
                              std::optional<std::string> two = std::nullopt)
                              :_thread_id(__thread_id), _chat_type(_type), _user_one(one), _user_two(two)
                    {
                    }

                    bool isGroupChat() const {
                              return _chat_type == UserChatType::GROUP;
                    }

                    static ChatThreadMeta createPrivateChat(const std::string& thread_id,
                              const std::string& user_one,
                              const std::string& user_two) {
                              return ChatThreadMeta(thread_id, UserChatType::PRIVATE, user_one, user_two);
                    }

                    static ChatThreadMeta createGroupChat(const std::string& thread_id) {
                              return ChatThreadMeta(thread_id, UserChatType::GROUP);
                    }

                    std::string _thread_id;
                    UserChatType _chat_type;

                    //those two variables are for private chat, if its a group chat then ignore them
                    std::optional<std::string> _user_one = std::nullopt;
                    std::optional<std::string> _user_two = std::nullopt;
          };
}

#endif //_CHATTINGTHREADDEF_H
