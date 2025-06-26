#include "addnewuserstackwidget.h"
#include "addusernamecardwidget.h"
#include "ui_addnewuserstackwidget.h"
#include <QListWidgetItem>
#include <authenticatenewfriendrequestdialog.h>
#include <tcpnetworkconnection.h>
#include <useraccountmanager.hpp>

/* define how many friend request are going to show up on list */
std::size_t AddNewUserStackWidget::FRIENDREQ_PER_PAGE = 9;

AddNewUserStackWidget::AddNewUserStackWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::AddNewUserStackWidget),
      m_curr_friend_requests_loaded(0) {
  ui->setupUi(this);

  registerSignal();
}

AddNewUserStackWidget::~AddNewUserStackWidget() { delete ui; }

void AddNewUserStackWidget::addNewWidgetItem(
    std::shared_ptr<UserFriendRequest> info) {
  /*allocate memory*/
  AddUserNameCardWidget *namecard = new AddUserNameCardWidget;

  /*transfer ownership*/
  namecard->setNameCardInfo(info);

  /* when user click add friend */
  connect(namecard, &AddUserNameCardWidget::signal_add_friend,
          [this](std::shared_ptr<UserFriendRequest> info) {
            qDebug() << "User agree to add this friend";
            AuthenticateNewFriendRequestDialog *auth =
                new AuthenticateNewFriendRequestDialog(this);
            auth->setModal(true);
            auth->setUserInfo(std::make_unique<UserNameCard>(
                info->sender_card.m_uuid, info->sender_card.m_avatorPath, info->sender_card.m_username,
                info->sender_card.m_nickname, info->sender_card.m_description, info->sender_card.m_sex));

            auth->show();
          });

  /*create item for Qlistwidget*/
  QListWidgetItem *item = new QListWidgetItem;

  /*set size and attributes*/
  item->setSizeHint(namecard->sizeHint());
  item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);

  /*start to insert item*/
  ui->friends_list->insertItem(0, item);
  ui->friends_list->setItemWidget(item, namecard);
}

void AddNewUserStackWidget::loadLimitedReqList() {
  auto new_list = UserAccountManager::get_instance()->getFriendRequestList(
      m_curr_friend_requests_loaded, FRIENDREQ_PER_PAGE);

  if (!new_list.has_value()) {
    return;
  }

  /* add it to the UI inf */
  for (const auto &item : new_list.value()) {
    addNewWidgetItem(item);
  }
}

void AddNewUserStackWidget::registerSignal() {
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_incoming_friend_request, this,
          &AddNewUserStackWidget::slot_incoming_friend_request);

  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_init_friend_request_list, this,
          &AddNewUserStackWidget::slot_init_friend_request_list);
}

NameCardWidgetShowList *AddNewUserStackWidget::getFriendListUI() const {
  return ui->friends_list;
}

/*server send friend request list to this client when user just finish login*/
void AddNewUserStackWidget::slot_init_friend_request_list() {
  loadLimitedReqList();
}

void AddNewUserStackWidget::slot_incoming_friend_request(
    std::optional<std::shared_ptr<UserFriendRequest>> info) {
  if (info.has_value()) {
    qDebug() << "Receive Friend Request From " << info.value()->sender_card.m_uuid;
    /*did the friend request sender send the request before?*/
    if (UserAccountManager::get_instance()->alreadyExistInRequestList(
            info.value()->sender_card.m_uuid)) {
      return;
    }

    /* add it to UI interface
     * addNewWidgetItem(info.value());
     * (Now Only added to the backup list, the loading signal will handle the
     * rest)
     */

    /*add it to the list*/
    UserAccountManager::get_instance()->addItem2List(info.value());
  }

  /*if there is nothing loaded perviously!!*/
  if (!m_curr_friend_requests_loaded) {
    loadLimitedReqList();
  }
}
