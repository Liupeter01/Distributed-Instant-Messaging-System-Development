#include "addusernamecardwidget.h"
#include <UserDef.hpp>
#include "ui_addusernamecardwidget.h"

AddUserNameCardWidget::AddUserNameCardWidget(QWidget *parent)
    : QFrame(parent), ui(new Ui::AddUserNameCardWidget) {
  ui->setupUi(this);

  /*registerSignal*/
  registerSignal();

  /*default settings*/
  loadDefaultSetting();
}

AddUserNameCardWidget::~AddUserNameCardWidget() { delete ui; }

void AddUserNameCardWidget::setNameCardInfo(
    std::shared_ptr<UserFriendRequest> info) {
  /*move ownership*/
  m_info = std::move(info);

  /*set avator qlabel*/
  Tools::loadImgResources({m_info->sender_card.m_avatorPath}, image_width, image_height,
                          "/static/");

  Tools::setQLableImage(ui->avator, m_info->sender_card.m_avatorPath, "/static/");

  ui->avator->setScaledContents(true);

  /*set friend name*/
  ui->friend_name->setText(m_info->sender_card.m_description);

  /*set last message*/
  ui->last_message->setText(m_info->sender_card.m_description);
}

const QSize AddUserNameCardWidget::getImageSize() {
  return QSize(image_width, image_height);
}

QSize AddUserNameCardWidget::sizeHint() const { return QSize(width, height); }

void AddUserNameCardWidget::registerSignal() {
  // connect button click signal<->slot
  connect(ui->add_button, &QPushButton::clicked, this, [this]() {
    ui->add_button->hide();
    ui->ignore_button->hide();
    ui->status_label->setText(QString("Added"));
    emit signal_add_friend(m_info);
  });

  connect(ui->ignore_button, &QPushButton::clicked, this, [this]() {
    ui->add_button->hide();
    ui->ignore_button->hide();
    ui->status_label->setText(QString("Ignored"));
  });
}

void AddUserNameCardWidget::loadDefaultSetting() {
  ui->add_button->show();
  ui->ignore_button->show();
  ui->status_label->hide();
}
