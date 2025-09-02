#include "chattingstackpage.h"
#include "chattingmsgitem.h"
#include "picturemsgbubble.h"
#include "textmsgbubble.h"
#include "ui_chattingstackpage.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <filetransferdialog.h>
#include <tcpnetworkconnection.h>
#include <useraccountmanager.hpp>

std::size_t ChattingStackPage::TXT_MSG_BUFFER_SIZE = 1024;

ChattingStackPage::ChattingStackPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::ChattingStackPage), m_text_msg_counter(0) {
  ui->setupUi(this);

  /*register signal*/
  registerSignal();

  /*load chattingstackpage*/
  Tools::loadImgResources(
      {"emoj_clicked.png", "emoj_hover.png", "emoj_normal.png"},
      ui->emo_label->width(), ui->emo_label->height());

  /*load chattingstackpage*/
  Tools::loadImgResources(
      {"file_clicked.png", "file_hover.png", "file_normal.png"},
      ui->file_label->width(), ui->file_label->height());

  /*set default image for chattingstackpage*/
  Tools::setQLableImage(ui->emo_label, "emoj_normal.png");
  Tools::setQLableImage(ui->file_label, "file_normal.png");
}

ChattingStackPage::~ChattingStackPage() { delete ui; }

void ChattingStackPage::registerSignal() {

  connect(ui->emo_label, &MultiClickableQLabel::clicked, this, [this]() {
    handle_clicked(ui->emo_label, "emoj_hover.png", "emoj_clicked.png");
  });

  connect(ui->emo_label, &MultiClickableQLabel::update_display, this, [this]() {
    handle_hover(ui->emo_label, "emoj_clicked.png", "emoj_hover.png",
                 "emoj_normal.png");
  });

  connect(ui->file_label, &MultiClickableQLabel::clicked, this, [this]() {
    handle_clicked(ui->file_label, "file_hover.png", "file_clicked.png");

    qDebug() << "Open File Transfer Dialog";

    FileTransferDialog *dialog = new FileTransferDialog(
        /*my id for verify*/ UserAccountManager::get_instance()
            ->getCurUserInfo(),
        /*chunk size = */ 2048, this);

    dialog->setModal(true);
    dialog->show();
  });

  connect(ui->file_label, &MultiClickableQLabel::update_display, this,
          [this]() {
            handle_hover(ui->file_label, "file_clicked.png", "file_hover.png",
                         "file_normal.png");
          });
}

void ChattingStackPage::handle_clicked(MultiClickableQLabel *label,
                                       const QString &hover,
                                       const QString &clicked) {
  auto state = label->getState();
  if (state.visiable == LabelState::VisiableStatus::ENABLED) {
    Tools::setQLableImage(label, clicked);
  } else {
    Tools::setQLableImage(label, hover);
  }
}

void ChattingStackPage::handle_hover(MultiClickableQLabel *label,
                                     const QString &click, const QString &hover,
                                     const QString &normal) {
  auto state = label->getState();
  if (state.hover == LabelState::HoverStatus::ENABLED) {
    Tools::setQLableImage(label, state.visiable ? click : hover);
  } else {
    Tools::setQLableImage(label, state.visiable ? click : normal);
  }
}

bool ChattingStackPage::isThreadSwitchingNeeded(
    const QString &target_uuid) const {

  /*friendinfo doesn't load any friend info yet*/
  if (!m_curFriendIdentity) {
    return true;
  }
  return !(m_curFriendIdentity->m_uuid == target_uuid);
}

void ChattingStackPage::switchChattingThread(
    std::shared_ptr<UserChatThread> user_thread) {

  /*set chatting dlghistory for current friend conversation thread info*/
  if (isThreadSwitchingNeeded(m_curFriendIdentity->m_uuid)) {

    m_curFriendIdentity = user_thread->getUserNameCard();

    /*replace placeholder text*/
    ui->friend_name->setText(m_curFriendIdentity->m_nickname);

    setChattingThreadData(user_thread);
  }
}

void ChattingStackPage::updateChattingUI(std::shared_ptr<UserChatThread> data) {
  setChattingThreadData(data);
}

bool ChattingStackPage::isChatValid(const MsgType type) {
  return type == MsgType::DEFAULT ? false : true;
}

const ChattingRole
ChattingStackPage::getRole(std::shared_ptr<ChattingBaseType> value) {
  return UserAccountManager::get_instance()->getCurUserInfo()->m_uuid ==
                 value->sender_uuid
             ? ChattingRole::Sender
             : ChattingRole::Receiver;
}

std::optional<ChattingMsgItem *>
ChattingStackPage::setupChattingMsgItem(const ChattingRole role) {

  QString username, pixmap_path;

  if (role == ChattingRole::Sender) {
    auto curUserInfo = UserAccountManager::get_instance()->getCurUserInfo();
    username = curUserInfo->m_nickname;
    pixmap_path = curUserInfo->m_avatorPath;

  } else {

    auto opt = UserAccountManager::get_instance()->findAuthFriendsInfo(
        m_curFriendIdentity->m_uuid);
    if (!opt.has_value()) {
      qDebug() << "User Friend Info Not Found!";
      return std::nullopt;
    }
    auto namecard = opt.value();
    username = namecard->m_nickname;
    pixmap_path = namecard->m_avatorPath;
  }

  ChattingMsgItem *item = new ChattingMsgItem(role);
  if (!item)
    return std::nullopt;

  item->setupUserName(username);
  item->setupIconPixmap(QPixmap(pixmap_path));
  return item;
}

void ChattingStackPage::setupBubbleFrameOnItem(
    const ChattingRole role, const MsgType type, ChattingMsgItem *item,
    std::shared_ptr<ChattingBaseType> value) {
  // Create Bubble Frame in UI
  QWidget *bubble{};

  if (!item)
    return;

  if (type == MsgType::TEXT) {
    bubble = new TextMsgBubble(role, value->getMsgContent());
  } else if (type == MsgType::IMAGE) {
    bubble = new PictureMsgBubble(role, value->getMsgContent());
  } else if (type == MsgType::AUDIO) {
  } else if (type == MsgType::VIDEO) {
  } else if (type == MsgType::FILE) {
  }

  if (!bubble)
    return;
  item->setupBubbleWidget(bubble);

  if (value->isOnLocal()) {
    item->setupMsgStatus(MessageStatus::UNSENT);
  } else {
    /*maybe more logic in the future*/
    item->setupMsgStatus(MessageStatus::SENT);
  }

  ui->chatting_record->pushBackItem(item);
}

void ChattingStackPage::distribute(std::shared_ptr<ChattingBaseType> value) {

  const auto type = value->getMsgType();
  const auto role = getRole(value);

  if (!isChatValid(type)) {
    qDebug() << "Invalid ChattingMsgType!";
    return;
  }

  // Set ChattingMsgItem
  auto msg_item = setupChattingMsgItem(role);
  if (!msg_item.has_value()) {
    qDebug() << "Sth Went Wrong In setupChattingMsgItem";
    return;
  }

  // Set Bubble Frame
  setupBubbleFrameOnItem(role, type, *msg_item, value);
}

void ChattingStackPage::setChattingThreadData(
    std::shared_ptr<UserChatThread> history) {

  /*remove all items*/
  ui->chatting_record->removeAllItem();

  auto all = history->dumpAll();

  for (const auto &value : all)
    distribute(value);
}

void ChattingStackPage::on_send_message_clicked() {

  QJsonObject obj;
  QJsonArray array;

  std::optional<QString> opt =
      UserAccountManager::get_instance()->getThreadIdByUUID(
          m_curFriendIdentity->m_uuid);
  if (!opt.has_value()) {
    qDebug() << "Friend Info Not Found! No Related UUID Found!";
    return;
  }

  const QVector<MsgInfo> &list = ui->user_input->getMsgList();

  auto thread_id = opt.value();

  for (std::size_t index = 0; index < list.size(); ++index) {
    /*currently, we are the msssage sender*/

    MsgInfo info = list[index];

    /*msg sender and msg receiver identity*/
    obj["msg_sender"] =
        UserAccountManager::get_instance()->getCurUserInfo()->m_uuid;
    obj["msg_receiver"] = m_curFriendIdentity->m_uuid;

    if (info.type == MsgType::TEXT) {
      // bubble_send = new TextMsgBubble(ChattingRole::Sender, info.content);

      /*text msg buffer counter*/

      if (m_text_msg_counter + info.content.length() > TXT_MSG_BUFFER_SIZE) {
        QJsonObject text_obj;
        text_obj["thread_id"] = thread_id;
        text_obj["text_sender"] = obj["msg_sender"];
        text_obj["text_receiver"] = obj["msg_receiver"];
        text_obj["text_msg"] = array;

        /*clean all array value*/
        array = QJsonArray{};
        text_obj = QJsonObject{};

        /*clear this counter*/
        m_text_msg_counter = 0;

        /*after connection to server, send TCP request*/
        TCPNetworkConnection::send_buffer(
            ServiceType::SERVICE_TEXTCHATMSGREQUEST, std::move(obj));
      }

      /*
       * generate an unique uuid for this message
       * and also, this message will be store in local storge
       * waiting for confirm from server!
       */
      obj["unique_id"] = QUuid::createUuid().toString();

      /*send message*/
      obj["msg_content"] = QString::fromUtf8(info.content.toUtf8());

      /*append current data to array*/
      array.append(obj);

      /*update counter*/
      m_text_msg_counter += info.content.length();
    } else if (info.type == MsgType::IMAGE) {
    } else if (info.type == MsgType::FILE) {
    }

    /*
     * although the messages which are sent will appear on the
     * chattingstackpage, the message will be marked as local data
     */

    std::shared_ptr<ChattingRecordBase> data =
        UserChatThread::generatePackage(info.type, obj);
    emit signal_append_chat_message(thread_id, data);
  }

  /* Ensure all data are sended! */
  QJsonObject text_obj;
  text_obj["thread_id"] = thread_id;
  text_obj["text_sender"] = obj["msg_sender"];
  text_obj["text_receiver"] = obj["msg_receiver"];
  text_obj["text_msg"] = array;

  /*after connection to server, send TCP request*/
  TCPNetworkConnection::send_buffer(ServiceType::SERVICE_TEXTCHATMSGREQUEST,
                                    std::move(text_obj));

  /*clean all array value*/
  array = QJsonArray{};
  text_obj = QJsonObject{};

  /*clear this counter*/
  m_text_msg_counter = 0;
}

void ChattingStackPage::on_clear_message_clicked() { ui->user_input->clear(); }
