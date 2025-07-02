#include "chattinghistorywidget.h"
#include "tools.h"

ChattingHistoryWidget::ChattingHistoryWidget(QWidget *parent)
    : ListItemWidgetBase(parent), ui(new Ui::ChattingHistoryWidget) {
  ui->setupUi(this);

  /*set item type to chatting history*/
  this->setItemType(ListItemType::ChattingHistory);
}

ChattingHistoryWidget::~ChattingHistoryWidget() { delete ui; }

void ChattingHistoryWidget::setUserInfo(
    std::shared_ptr<UserNameCard> info) {
  /*store the friendchattinghistory obj*/
  m_userinfo = info;

  updateLastMsg();
}

void ChattingHistoryWidget::updateLastMsg() {
  //setLastMessage<ChattingTextMsg>();
}

void ChattingHistoryWidget::setItemDisplay() {
  QSize size = ui->user_avator->size();
  // auto image =
  //     Tools::loadImages(m_userinfo->m_avatorPath, size.width(),
  //     size.height())
  //         .value();
  // ui->user_avator->setPixmap(QPixmap::fromImage(image));
  ui->user_name->setText(m_userinfo->m_nickname);
}

std::shared_ptr<UserNameCard>
ChattingHistoryWidget::getFriendsInfo(){
  return m_userinfo;
}
