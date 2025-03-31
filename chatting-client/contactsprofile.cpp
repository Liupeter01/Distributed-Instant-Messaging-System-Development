#include "contactsprofile.h"
#include "ui_contactsprofile.h"
#include <QDebug>
#include <QPixmap>
#include <UserNameCard.h>
#include <tools.h>

ContactsProfile::ContactsProfile(QWidget *parent)
    : QWidget(parent), ui(new Ui::ContactsProfile) {
  ui->setupUi(this);

  registerSignals();

  /*load qicon for ContactsProfile*/
  Tools::loadIconResources({"msg_chat_normal.png", "msg_chat_hover.png",
                            "msg_chat_press.png", "voice_chat_normal.png",
                            "voice_chat_hover.png", "voice_chat_press.png",
                            "video_chat_normal.png", "video_chat_hover.png",
                            "video_chat_press.png"});

  /*set default button image*/
  Tools::setPushButtonIcon(ui->msg, "msg_chat_normal.png");
  Tools::setPushButtonIcon(ui->voice, "voice_chat_normal.png");
  Tools::setPushButtonIcon(ui->video, "video_chat_normal.png");
}

ContactsProfile::~ContactsProfile() { delete ui; }

void ContactsProfile::setUserInfo(std::shared_ptr<UserNameCard> info) {
  m_userInfo = info;

  Tools::setQLableImage(ui->avator, m_userInfo->m_avatorPath, "/static");

  ui->nickname->setText(m_userInfo->m_nickname);
  ui->username->setText(m_userInfo->m_username);
}

void ContactsProfile::registerSignals() {

  /*switch three types of state(normal, hover, click)*/
  connect(ui->msg, &ButtonDisplaySwitching::clicked, this,
          &ContactsProfile::updateMsgChatButton);

  connect(ui->msg, &ButtonDisplaySwitching::update_display, this,
          &ContactsProfile::updateMsgChatButton);

  connect(ui->voice, &ButtonDisplaySwitching::clicked, this,
          &ContactsProfile::updateVoiceChatButton);

  connect(ui->voice, &ButtonDisplaySwitching::update_display, this,
          &ContactsProfile::updateVoiceChatButton);

  connect(ui->video, &ButtonDisplaySwitching::clicked, this,
          &ContactsProfile::updateVideoChatButton);

  connect(ui->video, &ButtonDisplaySwitching::update_display, this,
          &ContactsProfile::updateVideoChatButton);
}

void ContactsProfile::updateMsgChatButton() {
  auto state = ui->msg->getState();
  if (state.select == PushButtonState::SelectedStatus::ENABLED) {
    setCursor(Qt::PointingHandCursor);
    Tools::setPushButtonIcon(ui->msg, "msg_chat_press.png");
  } else {
    /*currently, its not selected! switch to hover
     *if it is not hovered! then switch to normal
     */
    Tools::setPushButtonIcon(
        ui->msg, state.hover == PushButtonState::HoverStatus::DISABLED
                     ? "msg_chat_normal.png"
                     : "msg_chat_hover.png");

    if (state.hover == PushButtonState::HoverStatus::ENABLED) {
      setCursor(Qt::PointingHandCursor);
    } else {
      unsetCursor();
    }
  }
}

void ContactsProfile::updateVoiceChatButton() {
  auto state = ui->voice->getState();
  if (state.select == PushButtonState::SelectedStatus::ENABLED) {
    setCursor(Qt::PointingHandCursor);
    Tools::setPushButtonIcon(ui->voice, "voice_chat_press.png");
  } else {
    /*currently, its not selected! switch to hover
     *if it is not hovered! then switch to normal
     */
    Tools::setPushButtonIcon(
        ui->voice, state.hover == PushButtonState::HoverStatus::DISABLED
                       ? "voice_chat_normal.png"
                       : "voice_chat_hover.png");

    if (state.hover == PushButtonState::HoverStatus::ENABLED) {
      setCursor(Qt::PointingHandCursor);
    } else {
      unsetCursor();
    }
  }
}

void ContactsProfile::updateVideoChatButton() {
  auto state = ui->video->getState();
  if (state.select == PushButtonState::SelectedStatus::ENABLED) {
    setCursor(Qt::PointingHandCursor);
    Tools::setPushButtonIcon(ui->video, "video_chat_press.png");
  } else {
    /*currently, its not selected! switch to hover
     *if it is not hovered! then switch to normal
     */
    Tools::setPushButtonIcon(
        ui->video, state.hover == PushButtonState::HoverStatus::DISABLED
                       ? "video_chat_normal.png"
                       : "video_chat_hover.png");

    if (state.hover == PushButtonState::HoverStatus::ENABLED) {
      setCursor(Qt::PointingHandCursor);
    } else {
      unsetCursor();
    }
  }
}

void ContactsProfile::on_msg_clicked() {
  qDebug() << "on_msg_clicked";
  emit signal_switch_chat_item(m_userInfo);
}

void ContactsProfile::on_voice_clicked() { qDebug() << "on_voice_clicked"; }

void ContactsProfile::on_video_clicked() { qDebug() << "on_video_clicked"; }
