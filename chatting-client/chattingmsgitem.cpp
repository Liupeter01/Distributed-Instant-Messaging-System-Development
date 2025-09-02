#include "chattingmsgitem.h"
#include <tools.h>

ChattingMsgItem::ChattingMsgItem(ChattingRole role, QWidget *parent)
    : m_role(role), QWidget{parent}, m_font("Microsoft YaHei"),
      m_nameLabel(new QLabel), m_iconLabel(new QLabel), m_grid(new QGridLayout),
      m_statusLabel(new QLabel),
      m_spacer(new QSpacerItem(40, 20, QSizePolicy::Expanding,
                               QSizePolicy::Minimum)),
      m_bubble(new QWidget) {

  /*load qimage*/
  Tools::loadImgResources({"read.png", "unread.png", "send_fail.png"},
                          statusLabel_width, statusLabel_height);

  m_font.setPointSize(10);
  m_nameLabel->setObjectName("msg_item_username");
  m_nameLabel->setFont(m_font);
  m_nameLabel->setFixedHeight(20);

  m_iconLabel->setScaledContents(true);
  m_iconLabel->setFixedSize(icon_width, icon_height);

  /*the interval of each widget*/
  m_grid->setVerticalSpacing(3);
  m_grid->setHorizontalSpacing(3);
  m_grid->setContentsMargins(3, 3, 3, 3);

  /*add message status(read/not read/ error)*/
  m_statusLabel->setFixedSize(statusLabel_width, statusLabel_height);
  m_statusLabel->setScaledContents(true);

  /*now we are the message sender*/
  if (role == ChattingRole::Sender) {
    /*          0             1               2              3
     * 0: |------------|------------|     namelabel    | iconlabel |
     * 1: |<--spacer-->| statusLabel|chattingmsgbubble | iconlabel |
     */
    m_nameLabel->setContentsMargins(0, 0, 8, 0);
    m_nameLabel->setAlignment(Qt::AlignRight);

    /*add bubble to layout*/
    m_grid->addWidget(m_bubble, 1, 2, 1, 1);

    /*add name qlabel*/
    m_grid->addWidget(m_nameLabel, 0, 2, 1, 1);

    /*add icon qlabel*/
    m_grid->addWidget(m_iconLabel, 0, 3, 2, 1, Qt::AlignTop);

    /*add status label*/
    m_grid->addWidget(m_statusLabel, 1, 1, 1, 1, Qt::AlignCenter);

    m_grid->addItem(m_spacer, 1, 0, 1, 1);

    m_grid->setColumnStretch(0, 2); // for col: 0
    m_grid->setColumnStretch(1,
                             0); // for col: 1(statusLabel should be a fix size)
    m_grid->setColumnStretch(
        2, 3); // for col: 2(m_bubble could be extend on both direction)
    m_grid->setColumnStretch(3, 0); // for col: 3

  } else {

    /*         0                1               2
     * 0: | iconlabel |     namelabel     |-----------|
     * 1: | iconlabel | chattingmsgbubble |<--spacer-->|
     */
    m_nameLabel->setContentsMargins(8, 0, 0, 0);
    m_nameLabel->setAlignment(Qt::AlignLeft);

    /*add bubble to layout*/
    m_grid->addWidget(m_bubble, 1, 1, 1, 1);

    /*add name qlabel*/
    m_grid->addWidget(m_nameLabel, 0, 1, 1, 1);

    /*add icon qlabel*/
    m_grid->addWidget(m_iconLabel, 0, 0, 2, 1, Qt::AlignTop);

    m_grid->addItem(m_spacer, 1, 2, 1, 1);

    m_grid->setColumnStretch(1, 3);
    m_grid->setColumnStretch(2, 2);
  }
  this->setLayout(m_grid);
}

ChattingMsgItem::~ChattingMsgItem() {
  delete m_nameLabel;
  delete m_iconLabel;
  delete m_grid;
}

void ChattingMsgItem::setupUserName(const QString &name) {
  m_nameLabel->setText(name);
}

void ChattingMsgItem::setupIconPixmap(const QPixmap &icon) {
  m_iconLabel->setPixmap(icon);
}

void ChattingMsgItem::setupBubbleWidget(QWidget *bubble) {
  QGridLayout *layout = reinterpret_cast<QGridLayout *>(this->layout());

  /**/
  layout->replaceWidget(m_bubble, bubble);

  /*avoid memeory leak*/
  if (nullptr != m_bubble) {
    delete m_bubble;
  }
  m_bubble = bubble;
}

void ChattingMsgItem::setupMsgStatus(const MessageStatus status) {

  if (!m_statusLabel) {
    qDebug() << "status label init failed!\n";
    return;
  }

  if(status == MessageStatus::UNSENT){

  }
  else if (status == MessageStatus::SENT) {
    Tools::setQLableImage(m_statusLabel, "unread.png");
  } else if (status == MessageStatus::READ) {
    Tools::setQLableImage(m_statusLabel, "read.png");
  } else if (status == MessageStatus::FAILED) {
    Tools::setQLableImage(m_statusLabel, "send_fail.png");
  }
}

void ChattingMsgItem::addStyleSheet() {
  /*setup style sheet for username display*/
  m_nameLabel->setStyleSheet("#msg_item_username{color:black;font-size:14px;"
                             "font-family: \"Microsoft YaHei\"");
}
