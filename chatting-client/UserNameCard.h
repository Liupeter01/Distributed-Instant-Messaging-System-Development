#ifndef USERNAMECARD_H
#define USERNAMECARD_H
#include <QString>
#include <cstdlib>
#include <memory>

enum class Sex { Male, Female };

/*record the critical info of the user*/
struct UserNameCard {

  UserNameCard(std::shared_ptr<UserNameCard> card)
      : m_sex(card->m_sex), m_uuid(card->m_uuid), m_username(card->m_username),
        m_nickname(card->m_nickname), m_description(card->m_description),
        m_avatorPath(card->m_avatorPath) {}

  UserNameCard(const UserNameCard &card)
      : m_sex(card.m_sex), m_uuid(card.m_uuid), m_username(card.m_username),
        m_nickname(card.m_nickname), m_description(card.m_description),
        m_avatorPath(card.m_avatorPath) {}

  UserNameCard(UserNameCard &&card)
      : m_sex(card.m_sex), m_uuid(card.m_uuid), m_username(card.m_username),
        m_nickname(card.m_nickname), m_description(card.m_description),
        m_avatorPath(card.m_avatorPath) {}

  UserNameCard(const QString &uuid, const QString &avator_path,
               const QString &username, const QString &nickname,
               const QString &desc, Sex sex)
      : m_sex(sex), m_uuid(uuid), m_username(username), m_nickname(nickname),
        m_description(desc), m_avatorPath(avator_path) {}

  Sex m_sex;
  QString m_uuid;
  QString m_avatorPath;
  QString m_username;
  QString m_nickname;
  QString m_description;
};

#endif // USERNAMECARD_H
