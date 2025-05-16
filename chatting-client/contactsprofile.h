#ifndef CONTACTSPROFILE_H
#define CONTACTSPROFILE_H

#include <QWidget>

namespace Ui {
class ContactsProfile;
}

/*declaration*/
struct UserNameCard;

class ContactsProfile : public QWidget {
  Q_OBJECT

public:
  explicit ContactsProfile(QWidget *parent = nullptr);
  virtual ~ContactsProfile();

public:
  void setUserInfo(std::shared_ptr<UserNameCard> info);

private:
  void registerSignals();

  /*switch three types of state(normal, hover, click)*/
  void updateMsgChatButton();
  void updateVoiceChatButton();
  void updateVideoChatButton();

signals:
  void signal_switch_chat_item(std::shared_ptr<UserNameCard> info);

private slots:
  void on_msg_clicked();
  void on_voice_clicked();
  void on_video_clicked();

private:
  Ui::ContactsProfile *ui;

  /*user name card*/
  std::shared_ptr<UserNameCard> m_userInfo;
};

#endif // CONTACTSPROFILE_H
