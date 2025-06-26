#pragma once
#ifndef _USERDEF_HPP
#define _USERDEF_HPP
#include <string>
#include <optional>
#include <vector>

namespace user {

          enum class Sex { Male, Female, Unkown};

          struct UserNameCard {
                    Sex m_sex;
                    std::string m_uuid;
                    std::string m_avatorPath;
                    std::string m_username;
                    std::string m_nickname;
                    std::string m_description;

                    UserNameCard() = default;
                    UserNameCard(const std::string& friend_uuid)
                              :UserNameCard(friend_uuid, "", "", "", "", Sex::Unkown)
                    {
                    }

                    UserNameCard(const UserNameCard& card)
                              : m_sex(card.m_sex), m_uuid(card.m_uuid), m_username(card.m_username),
                              m_nickname(card.m_nickname), m_description(card.m_description),
                              m_avatorPath(card.m_avatorPath) {
                    }

                    UserNameCard(const std::string& uuid, const std::string& avator_path,
                              const std::string& username, const std::string& nickname,
                              const std::string& desc, Sex sex)
                              : m_sex(sex), m_uuid(uuid), m_username(username), m_nickname(nickname),
                              m_description(desc), m_avatorPath(avator_path) {
                    }
          };

          struct UserFriendRequest {
                    UserNameCard sender_card;
                    std::string receiver_uuid;
                    std::string request_message;

                    UserFriendRequest(const std::string& from, const std::string& to,
                              const std::string& nick, const std::string& msg,
                              const std::string& avator_path, const std::string& username,
                              const std::string& desc, Sex sex)
                              : sender_card(from, avator_path, username, nick, desc, sex),
                              receiver_uuid(to), request_message(msg) {
                    }

                    const UserNameCard& getNameCard() const {
                              return sender_card;
                    }
          };
}

#endif // _USERDEF_HPP