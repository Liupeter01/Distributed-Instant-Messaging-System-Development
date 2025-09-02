#ifndef CHATTINGCONTACTLIST_H
#define CHATTINGCONTACTLIST_H

#include "mainframeshowlists.h"
#include <QEvent>
#include <QListWidget>
#include <optional>

/*declaration*/
class QListWidgetItem;
class ChattingContactItem;
struct UserNameCard;

class ChattingContactList : public MainFrameShowLists {
  Q_OBJECT

public:
  ChattingContactList(QWidget *parent = nullptr);
  virtual ~ChattingContactList();

public:
  /*set AddUserWidget*/
  void addAddUserWidget();

  /*set chatting contact info*/
  void addChattingContact(std::shared_ptr<UserNameCard> info);

  /*set dialog with seperator*/
  void addGroupSeperator(const QString &text);

  /*retrieve limited number of contact list from UserAccountManager*/
  void loadLimitedContactsList();

signals:
  /*connect with ChattingDlgMainFrame::switchNewUserPage*/
  void signal_switch_addnewuser();

  /*if target user has already became a auth friend with current user*/
  void signal_switch_user_profile(std::shared_ptr<UserNameCard> info);

private slots:
  /*user click one of the contact*/
  void slot_itemClicked(QListWidgetItem *item);

  /*server be able to send authenticate friend list to this client*/
  void slot_init_auth_friend_list();

private:
  /*define how many contact are going to show up on contact list*/
  static std::size_t CONTACT_PER_PAGE;
  std::size_t m_curr_contact_person_loaded = 0;

  /*signal<=>slot*/
  void registerSignal();

private:
  const QString static_text;
};

#endif // CHATTINGCONTACTLIST_H
