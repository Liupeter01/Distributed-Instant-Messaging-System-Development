#include "authenticatenewfriendrequestdialog.h"
#include "addusernamecarddialog.h"
#include "onceclickableqlabel.h"
#include "tcpnetworkconnection.h"
#include "tools.h"
#include "ui_authenticatenewfriendrequestdialog.h"
#include <ByteOrderConverter.hpp>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollBar>
#include <useraccountmanager.hpp>

AuthenticateNewFriendRequestDialog::AuthenticateNewFriendRequestDialog(
    QWidget *parent)
    : QDialog(parent), ui(new Ui::AuthenticateNewFriendRequestDialog),
      prefix_string(QString("New Tag: ")),
      m_existing_cur_pos(
          QPoint(COMPENSATION_WIDTH,
                 COMPENSATION_HEIGHT)) /*init existing tag current pos*/
      ,
      m_selected_cur_pos(
          QPoint(COMPENSATION_WIDTH, COMPENSATION_HEIGHT)) /*init selected tag*/
{
  ui->setupUi(this);

  /*register signal<->slot*/
  registerSignal();

  /*set up placeholder text*/
  setupDefaultInfo();

  /*set up ui style*/
  setupWindowStyle();

  /*test*/
  loadtestFunction();

  /*load image for usertag widget*/
  Tools::loadImgResources({"unselect_tag.png"},
                          UserTagWidget::getImageSize().width(),
                          UserTagWidget::getImageSize().height());
}

AuthenticateNewFriendRequestDialog::~AuthenticateNewFriendRequestDialog() {
  delete ui;
}

/*
 * transfer UserNameCard structure to current class
 * We need to send request by using this stucture
 */
void AuthenticateNewFriendRequestDialog::setUserInfo(
    std::unique_ptr<UserNameCard> card) {
  m_info = std::move(card);
}

void AuthenticateNewFriendRequestDialog::registerSignal() {
  /*bind with show more label click event*/
  connect(ui->show_more_label, &OnceClickableQLabel::clicked, this,
          &AuthenticateNewFriendRequestDialog::slot_show_more_label);

  /*bind with press enter inside tag_input widget*/
  connect(ui->tag_input, &RestrictUserSearchingInput::returnPressed, this,
          &AuthenticateNewFriendRequestDialog::slot_input_tag_press_enter);

  /*bind with textchange inside tag_input*/
  connect(ui->tag_input, &RestrictUserSearchingInput::textChanged, this,
          &AuthenticateNewFriendRequestDialog::slot_input_tag_textchange);

  /*bind with editfinished event inside tag_input*/
  connect(ui->tag_input, &RestrictUserSearchingInput::editingFinished, this,
          &AuthenticateNewFriendRequestDialog::slot_input_tag_finished);

  connect(ui->tag_display, &OnceClickableQLabel::clicked, this,
          &AuthenticateNewFriendRequestDialog::slot_choose_tag_by_click);
}

void AuthenticateNewFriendRequestDialog::setupDefaultInfo() {
  /**limit user input ammount to 21**/
  ui->nick_name_edit->setMaxLength(21);
  ui->tag_input->setMaxLength(21);

  /*input label
   * 1. setup start pos
   * 2. setup widget height
   */
  ui->tag_input->move(2, 2);
  ui->tag_input->setFixedHeight(20);

  /*hide status display bar*/
  ui->user_tag_display_bar->hide();

  ui->nick_name_edit->setPlaceholderText(QString("test_friend"));
  ui->tag_input->setPlaceholderText(QString("Select or search tags"));
}

void AuthenticateNewFriendRequestDialog::setupWindowStyle() {
  /*hide dialog title*/
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

  ui->scrollArea->horizontalScrollBar()->setHidden(true);
  ui->scrollArea->verticalScrollBar()->setHidden(true);
  ui->scrollArea->installEventFilter(this);

  this->setModal(true);
}

bool AuthenticateNewFriendRequestDialog::eventFilter(QObject *object,
                                                     QEvent *event) {
  if (object == ui->scrollArea) {
    if (event->type() == QEvent::Enter) {
      ui->scrollArea->verticalScrollBar()->setHidden(false);
    } else if (event->type() == QEvent::Leave) {
      ui->scrollArea->verticalScrollBar()->setHidden(true);
    }
  }
  return QDialog::eventFilter(object, event);
}

void AuthenticateNewFriendRequestDialog::addNewTag2Container(
    const QString &text) {
  /*store all tags*/
  auto it = std::find(m_tagLists.begin(), m_tagLists.end(), text);
  if (it == m_tagLists.end()) {
    m_tagLists.push_back(text);
  }
}

void AuthenticateNewFriendRequestDialog::addNewTag2InputTag(
    const QString &text) {
  /*clear input text*/
  ui->tag_input->clear();

  /*we find it inside select key*/
  auto it = std::find(m_selected_key.begin(), m_selected_key.end(), text);
  if (it != m_selected_key.end()) {
    return;
  }

  /*create a new widget*/
  UserTagWidget *tag(new UserTagWidget(ui->input_tag_widget));
  tag->setTagName(text);
  tag->setObjectName("UserDefTag");

  // connect signal slot for close signal
  connect(tag, &UserTagWidget::signal_close, this,
          &AuthenticateNewFriendRequestDialog::slot_remove_selected_tag);

  /*add new tag to the vector container*/
  createSelectedTag(text, tag);

  /*calculate do we need to generate this object at a new line*/
  if (m_selected_cur_pos.x() + tag->width() > ui->input_tag_widget->width()) {
    m_selected_cur_pos.setX(COMPENSATION_WIDTH);
    m_selected_cur_pos.setY(m_selected_cur_pos.y() + tag->height() +
                            COMPENSATION_HEIGHT);
  }

  /*move it to new place*/
  tag->move(m_selected_cur_pos);
  tag->show();

  /*update m_selected_cur_pos to the next new place*/
  m_selected_cur_pos.setX(m_selected_cur_pos.x() + tag->width() +
                          COMPENSATION_WIDTH);
  if (m_selected_cur_pos.x() + COMPENSATION_WIDTH >
      ui->input_tag_widget->width()) {
    /*move tag_input widget position to avoid collison with UserTagWidget*/
    m_selected_cur_pos.setX(COMPENSATION_WIDTH);
    m_selected_cur_pos.setY(m_selected_cur_pos.y() + tag->height() +
                            COMPENSATION_HEIGHT);
  }

  /*move tag_input's position, adjust height*/
  ui->tag_input->setFixedHeight(tag->height() + COMPENSATION_HEIGHT);
  ui->tag_input->move(m_selected_cur_pos);

  /*extend the height of input tag widget*/
  if (ui->input_tag_widget->height() <
      m_selected_cur_pos.y() + tag->height() + COMPENSATION_HEIGHT) {
    ui->input_tag_widget->setFixedHeight(
        m_selected_cur_pos.y() + tag->height() * 2 + COMPENSATION_HEIGHT);
  }
}

void AuthenticateNewFriendRequestDialog::addNewTag2ExistingTag(
    const QString &text) {
  /*we find it inside existing key*/
  auto it = std::find(m_existing_key.begin(), m_existing_key.end(), text);
  if (it != m_existing_key.end()) {
    return;
  }

  auto label = m_exist_label.find(text);
  if (label != m_exist_label.end()) {
    /*if we found this text, then set the status to selected*/
    label->second->setCurrentState(LabelState::VisiableStatus::ENABLED);
    return;
  }

  /*add a new onceclickableqlabel for display existing tag*/
  OnceClickableQLabel *exist = new OnceClickableQLabel(ui->existing_tag_widget);
  exist->setText(text);
  exist->adjustSize();
  exist->setObjectName("exist_tag");
  /*set size*/
  // exist->setFixedSize(text_width, text_height);
  exist->setCurrentState(
      LabelState::VisiableStatus::ENABLED); // set it to selected

  connect(exist, &OnceClickableQLabel::clicked, this,
          &AuthenticateNewFriendRequestDialog::slot_choose_tag_by_click);

  /*add new widget and text to specfic data structure*/
  createExistingTag(text, exist);

  QFontMetricsF fontMetrics(exist->font());

  auto text_width = fontMetrics.horizontalAdvance(exist->text());
  auto text_height = fontMetrics.height();

  qDebug() << "text_width = " << text_width << "\ntext_height = " << text_height
           << "\n";

  /*calculate do we need to generate this object at a new line*/
  if (m_existing_cur_pos.x() + text_width > ui->existing_tag_widget->width()) {
    m_existing_cur_pos.setX(COMPENSATION_WIDTH);
    m_existing_cur_pos.setY(m_existing_cur_pos.y() + text_height +
                            COMPENSATION_HEIGHT);
  }

  /*move it to new place*/
  exist->move(m_existing_cur_pos);
  exist->show();

  /*update m_selected_cur_pos to the newest status*/
  m_existing_cur_pos.setX(m_existing_cur_pos.x() + text_width +
                          2 * COMPENSATION_WIDTH);
  m_existing_cur_pos.setY(m_existing_cur_pos.y());

  /*extend the height of exisiting tag widget*/
  if (ui->existing_tag_widget->height() <
      m_selected_cur_pos.y() + text_height + COMPENSATION_HEIGHT) {
    ui->existing_tag_widget->setFixedHeight(
        m_selected_cur_pos.y() + text_height * 2 + COMPENSATION_HEIGHT);
  }
}

void AuthenticateNewFriendRequestDialog::createSelectedTag(
    const QString &text, UserTagWidget *widget) {
  m_selected_key.push_back(text);
  m_selected_label.insert(std::pair<QString, std::shared_ptr<UserTagWidget>>(
      text, std::shared_ptr<UserTagWidget>(widget, [](UserTagWidget *) {})));
}

void AuthenticateNewFriendRequestDialog::createExistingTag(
    const QString &text, OnceClickableQLabel *widget) {
  m_existing_key.push_back(text);
  m_exist_label.insert(std::pair<QString, std::shared_ptr<OnceClickableQLabel>>(
      text, std::shared_ptr<OnceClickableQLabel>(
                widget, [](OnceClickableQLabel *) {})));
}

void AuthenticateNewFriendRequestDialog::resetLabels() {}

void AuthenticateNewFriendRequestDialog::closeDialog() {
  qDebug() << "closing AuthenticateNewFriendRequestDialog";
  this->hide();
  deleteLater();
}

void AuthenticateNewFriendRequestDialog::loadtestFunction() {
  m_tagLists = {"Python",          "NodeJS",        "C",     "Java",
                "C++ Primer Plus", "Java",          "Block", "Family Member",
                "Classmate",       "COMPUTER gAmes"};

  for (const auto &i : m_tagLists) {
    addNewTag2ExistingTag(i);
  }
}

void AuthenticateNewFriendRequestDialog::on_confirm_button_clicked() {
  qDebug() << "[AuthenticateNewFriendRequestDialog::on_confirm_button_clicked]:"
              " User Confirm "
              "To Send Request\n";

  /*get friends alternativeName*/
  auto alternativeName = ui->nick_name_edit->text();
  if (alternativeName.isEmpty()) {
    alternativeName = ui->nick_name_edit->placeholderText();
  }

  /*generate json*/
  QJsonObject obj;
  obj["src_uuid"] = m_info->m_uuid; // the friend request sender
  obj["dst_uuid"] = UserAccountManager::get_instance()->get_uuid(); // my uuid
  obj["alternative_name"] = alternativeName;

  QJsonDocument doc(obj);

  /*it should be store as a temporary object, because send_buffer will modify
   * it!*/
  auto json_data = doc.toJson(QJsonDocument::Compact);

  SendNodeType send_buffer(
      static_cast<uint16_t>(ServiceType::SERVICE_FRIENDREQUESTCONFIRM),
      json_data, ByteOrderConverterReverse{});

  /*after connection to server, send TCP request*/
  TCPNetworkConnection::get_instance()->send_data(std::move(send_buffer));

  closeDialog();
}

void AuthenticateNewFriendRequestDialog::on_cancel_button_clicked() {
  qDebug() << "[AuthenticateNewFriendRequestDialog::on_cancel_button_clicked]: "
              "User Cancel "
              "Request\n";

  closeDialog();
}

void AuthenticateNewFriendRequestDialog::slot_show_more_label() {}

void AuthenticateNewFriendRequestDialog::slot_input_tag_press_enter() {
  auto text = ui->tag_input->text();

  /*no text*/
  if (text.isEmpty()) {
    return;
  }

  /*hide input notification widget*/
  ui->user_tag_display_bar->hide();

  /*store all tags*/
  addNewTag2Container(text);

  /*add it to new tag*/
  addNewTag2InputTag(text);

  /*add new tag to exisiting existing_tag_widget*/
  addNewTag2ExistingTag(text);
}

void AuthenticateNewFriendRequestDialog::slot_remove_selected_tag() {}

void AuthenticateNewFriendRequestDialog::slot_input_tag_textchange(
    const QString &text) {
  /*no text*/
  if (text.isEmpty()) {
    /*clear text*/
    ui->user_tag_display_bar->hide();
    ui->tag_display->setText(QString(""));
    return;
  }

  auto it = std::find(m_tagLists.begin(), m_tagLists.end(), text);
  auto show_string = (it != m_tagLists.end() ? text : prefix_string + text);

  ui->tag_display->setText(show_string);
  ui->user_tag_display_bar->show();
}

void AuthenticateNewFriendRequestDialog::slot_input_tag_finished() {}

void AuthenticateNewFriendRequestDialog::slot_choose_tag_by_click(
    QString str, LabelState state) {
  if (state.visiable != LabelState::VisiableStatus::ENABLED) {
    return;
  }
  /*condition 1
   * user click the user_tag_display_bar, and add it to both selected & existing
   * label
   */
  int index = str.indexOf(prefix_string);
  if (index != -1) {

    /* condition 1
     * user click the user_tag_display_bar, and add it to both selected &
     * existing label
     */
    auto text = str.mid(index + prefix_string.length());

    addNewTag2Container(text);

    /*add it to new tag*/
    addNewTag2InputTag(text);

    /*add new tag to exisiting existing_tag_widget*/
    addNewTag2ExistingTag(text);
  } else {
    /* condition 2
     * user choose a tag from existing tag, and add it to selected_label
     */
    addNewTag2Container(str);

    /*add it to new tag*/
    addNewTag2InputTag(str);
  }
}
