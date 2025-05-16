#include "chattingcontactitem.h"
#include "tools.h"
#include "ui_chattingcontactitem.h"

ChattingContactItem::ChattingContactItem(QWidget *parent)
    : static_text("Add New Friend"), ListItemWidgetBase(parent),
      ui(new Ui::ChattingContactItem) {
  ui->setupUi(this);

  /*set list item type, default value = MyContact*/
  setItemType(ListItemType::MyContact);

  /*load qimage for AddUserWidget*/
  Tools::loadImgResources({"add_friend_clicked.png"},
                          ui->notification_label->width(),
                          ui->notification_label->height());
}

ChattingContactItem::~ChattingContactItem() {}

const QSize ChattingContactItem::getImageSize() {
  return QSize(image_width, image_height);
}

QSize ChattingContactItem::sizeHint() const { return m_size; }

void ChattingContactItem::setAddUserWidget() {
  /*set list item type, AddUserWidget*/
  setItemType(ListItemType::AddUserWidget);

  /*setup text by static text*/
  ui->display_label->setText(static_text);

  /*move notification label to front*/
  ui->notification_label->raise();

  /*setup avator by static label*/
  Tools::setQLableImage(ui->notification_label, "add_friend_clicked.png");

  /*hide group zone*/
  ui->contact_zone->show();
  ui->group_zone->hide();

  /*update contact zone's size*/
  m_size = ui->contact_zone->size();
}

void ChattingContactItem::setChattingContact(
    std::shared_ptr<UserNameCard> info) {
  /*set list item type, ContactHistory*/
  setItemType(ListItemType::MyContact);

  /*record user's info*/
  m_userinfo = info;

  /*setup avator by static label*/
  Tools::setQLableImage(ui->notification_label, info->m_avatorPath, "/static/");

  /*setup text by static text*/
  ui->display_label->setText(info->m_nickname);

  /*move notification_label to front*/
  ui->notification_label->raise();

  /*hide group zone*/
  ui->contact_zone->show();
  ui->group_zone->hide();

  /*update contact zone's size*/
  m_size = ui->contact_zone->size();
}

std::shared_ptr<UserNameCard> ChattingContactItem::getChattingContact() {
  return m_userinfo;
}

void ChattingContactItem::setGroupSeperator(const QString &text) {
  /*set list item type, ShowContactByGroup*/
  setItemType(ListItemType::ShowContactByGroup);

  /*setup text*/
  ui->group_label->setText(text);

  /*hide contact zone*/
  ui->contact_zone->hide();
  ui->group_zone->show();

  /*set qss style*/
  ui->group_zone->setStyleSheet(QString("#group_zone{"
                                        "color:white;"
                                        "background-color:#d3d7d4;"
                                        "font-size:14px;"
                                        "font-family: \"Microsoft YaHei\";"
                                        "border: none;"
                                        "outline: none;"
                                        "}"));

  /*update group zone's size*/
  m_size = ui->group_zone->size();
}
