#include "chattinghistorywidget.h"
#include "tools.h"

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

void ChattingHistoryWidget::setLastMessage(const QString &msg) {
  ui->last_message->setText(msg);
}

void ChattingHistoryWidget::setItemDisplay(std::shared_ptr<UserNameCard> info) {

    setUserInfo(info);

  QSize size = ui->user_avator->size();
  // auto image =
  //     Tools::loadImages(m_userinfo->m_avatorPath, size.width(),
  //     size.height())
  //         .value();
  // ui->user_avator->setPixmap(QPixmap::fromImage(image));
  ui->user_name->setText(m_userinfo->m_nickname);
}

void ChattingHistoryWidget::setNewMessageArrival(bool status)
{

}

std::shared_ptr<UserNameCard> ChattingHistoryWidget::getFriendsInfo() {
  return m_userinfo;
}
