#pragma once
#ifndef _USERDEF_HPP
#define _USERDEF_HPP
#include <QString>
#include <optional>
#include <vector>

enum class Sex { Male, Female, Unkown };

struct UserNameCard {
  Sex m_sex;
  QString m_uuid;
  QString m_avatorPath;
  QString m_username;
  QString m_nickname;
  QString m_description;

  UserNameCard() = default;
  UserNameCard(const QString &friend_uuid)
      : UserNameCard(friend_uuid, "", "", "", "", Sex::Unkown) {}

  UserNameCard(const UserNameCard &card)
      : m_sex(card.m_sex), m_uuid(card.m_uuid), m_username(card.m_username),
        m_nickname(card.m_nickname), m_description(card.m_description),
        m_avatorPath(card.m_avatorPath) {}

  UserNameCard(const QString &uuid, const QString &avator_path,
               const QString &username, const QString &nickname,
               const QString &desc, Sex sex)
      : m_sex(sex), m_uuid(uuid), m_username(username), m_nickname(nickname),
        m_description(desc), m_avatorPath(avator_path) {}
};

struct UserFriendRequest {
  UserNameCard sender_card;
  QString receiver_uuid;
  QString request_message;

  UserFriendRequest(const QString &from, const QString &to, const QString &nick,
                    const QString &msg, const QString &avator_path,
                    const QString &username, const QString &desc, Sex sex)
      : sender_card(from, avator_path, username, nick, desc, sex),
        receiver_uuid(to), request_message(msg) {}

  const UserNameCard &getNameCard() const { return sender_card; }
};

#endif // _USERDEF_HPP
