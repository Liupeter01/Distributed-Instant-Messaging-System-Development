#include "chattingcontactlist.h"
#include "chattingcontactitem.h"
#include "tools.h"
#include <QListWidgetItem>
#include <QRandomGenerator>
#include <QScrollBar>
#include <tcpnetworkconnection.h>
#include <useraccountmanager.hpp>

/*Ui interface*/
#include <contactsprofile.h>

/* define how many contact are going to show up on contact list */
std::size_t ChattingContactList::CONTACT_PER_PAGE = 8;

ChattingContactList::ChattingContactList(QWidget *parent)
    : static_text("Add New Friend"), m_curr_contact_person_loaded(0),
      MainFrameShowLists(parent) {
  /*register signal*/
  registerSignal();

  /*add new friend button as a group*/
  addGroupSeperator(static_text);
  addAddUserWidget();

  /*show contact as a group*/
  addGroupSeperator(QString("My Contact"));

  /*load test user avator image im "/static/" dir*/
  Tools::loadImgResources({"0.png", "1.png", "2.png", "3.png", "4.png", "5.png",
                           "6.png", "7.png", "8.png"},
                          ChattingContactItem::getImageSize().width(),
                          ChattingContactItem::getImageSize().height(),
                          "/static/");
}

ChattingContactList::~ChattingContactList() {}

void ChattingContactList::registerSignal() {
  /*user click one of the contact, connect signal<->slot*/
  connect(this, &QListWidget::itemClicked, this,
          &ChattingContactList::slot_itemClicked);

  /*server be able to send authenticate friend list to this client*/
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_init_auth_friend_list, this,
          &ChattingContactList::slot_init_auth_friend_list);

}

void ChattingContactList::addAddUserWidget() {
  ChattingContactItem *add_widget(new ChattingContactItem);

  /*set AddUserWidget*/
  add_widget->setAddUserWidget();

  QListWidgetItem *item(new QListWidgetItem);
  item->setSizeHint(add_widget->sizeHint());

  this->addItem(item);
  this->setItemWidget(item, add_widget);

  /*set it by default*/
  this->setCurrentItem(item);
  this->update();
}

void ChattingContactList::addChattingContact(
    std::shared_ptr<UserNameCard> info) {
  ChattingContactItem *contact_widget(new ChattingContactItem);

  /*set chatting contact info*/
  contact_widget->setChattingContact(info);

  QListWidgetItem *item(new QListWidgetItem);
  item->setSizeHint(contact_widget->sizeHint());

  this->addItem(item);
  this->setItemWidget(item, contact_widget);
  this->update();
}

void ChattingContactList::addGroupSeperator(const QString &text) {
  ChattingContactItem *group_widget(new ChattingContactItem);

  /*set dialog with seperator*/
  group_widget->setGroupSeperator(text);

  QListWidgetItem *item(new QListWidgetItem);
  item->setSizeHint(group_widget->sizeHint());

  /*item should be non-clickable*/
  item->setFlags(item->flags() & ~Qt::ItemIsSelectable);

  this->addItem(item);
  this->setItemWidget(item, group_widget);
  this->update();
}

void ChattingContactList::slot_itemClicked(QListWidgetItem *item) {
  /*get widget base type*/
  ListItemWidgetBase *base =
      reinterpret_cast<ListItemWidgetBase *>(this->itemWidget(item));

  if (base == nullptr) {
    qDebug() << "ListItemWidgetBase is null!\n";
    return;
  }

  /*filter ListItemWidgetBase ItemType*/
  if (base->getItemType() == ListItemType::Default) {
    qDebug() << "invalid widget object!\n";
    return;
  }
  /*click Contact By Group*/
  else if (base->getItemType() == ListItemType::ShowContactByGroup) {
    qDebug() << "group widget is clicked, after click this we could hide the "
                "contact!\n";
    return;
  }
  /*click add user widget button*/
  else if (base->getItemType() == ListItemType::AddUserWidget) {
    qDebug() << "User Press Add New Friend Button On Contact List\n"
             << "Emit Signal And Switch To Auth Friend Page In "
                "Stackwidget(AddNewUserStackWidget)";

    emit signal_switch_addnewuser();
  }
  /*click Contact widget item*/
  else if (base->getItemType() == ListItemType::MyContact) {
    qDebug() << "User Press One Of The Contact In Contact List\n"
             << "Emit Signal And Switch To UserProfile Page In "
                "Stackwidget(ContactsProfile)";

    emit signal_switch_user_profile(
        reinterpret_cast<ChattingContactItem *>(base)->getChattingContact());
  }
}

void ChattingContactList::loadLimitedContactsList() {
  auto authFriend = UserAccountManager::get_instance()->getAuthFriendList(
      m_curr_contact_person_loaded, CONTACT_PER_PAGE);

  if (!authFriend.has_value()) {
    return;
  }

  for (const auto &item : authFriend.value()) {
    addChattingContact(item);
  }
}

/*
 * data has already loaded into DS in early TCPNetworkconnection class
 * so we only need to load limit number of them to the list
 */
void ChattingContactList::slot_init_auth_friend_list() {

  loadLimitedContactsList();
}
