#include "chattinghistorywidget.h"
#include "tools.h"
#include <useraccountmanager.hpp>

ChattingHistoryWidget::ChattingHistoryWidget(QWidget *parent)
    : ListItemWidgetBase(parent), ui(new Ui::ChattingHistoryWidget) {
  ui->setupUi(this);

  /*set item type to chatting history*/
  this->setItemType(ListItemType::ChattingHistory);
}

ChattingHistoryWidget::~ChattingHistoryWidget() { delete ui; }

void ChattingHistoryWidget::setUserInfo(std::shared_ptr<UserNameCard> info) {
  /*store the friendchattinghistory obj*/
  m_userinfo = info;
}

void ChattingHistoryWidget::setLastMessage() {

  /*no friend info found here*/
  if (!m_userinfo) {
    qDebug() << "Invalid Auth Friend Info!\n";
    return;
  }

  /*
   * because this is a incoming msg, so using sender uuid as friend uuid
   * Find This User's "thread_id", and try to locate history info
   */
  std::optional<QString> thread_id =
      UserAccountManager::get_instance()->getThreadIdByUUID(m_userinfo->m_uuid);

  // not found at all
  if (!thread_id.has_value()) {
    qDebug() << "No Matching ThreadID found releated to uuid!";
    return;
  }

  auto thread_opt = UserAccountManager::get_instance()->getChattingThreadData(
      thread_id.value());
  if (!thread_opt.has_value()) {
    qDebug() << "No Chatting Thread Data Found!";
    return;
  }
  auto thread = thread_opt.value();

  ui->last_message->setText(thread->getLastMsg()->getMsgContent());
}

void ChattingHistoryWidget::setItemDisplay(std::shared_ptr<UserNameCard> info) {

  setUserInfo(info);

  QSize size = ui->user_avator->size();

  Tools::loadAvatarResources(info, ui->user_avator);

  ui->user_name->setText(m_userinfo->m_nickname);

  // load the last message
  setLastMessage();
}

void ChattingHistoryWidget::setNewMessageArrival(bool status) {}

std::shared_ptr<UserNameCard> ChattingHistoryWidget::getFriendsInfo() {
  return m_userinfo;
}
