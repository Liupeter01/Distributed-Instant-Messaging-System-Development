#include "addusernamecarddialog.h"
#include "adduserrequestdialog.h"
#include "tools.h"
#include "ui_addusernamecarddialog.h"

AddUserNameCardDialog::AddUserNameCardDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AddUserNameCardDialog) {
  ui->setupUi(this);

  /*register signal slot*/
  registerSignal();

  /*set window style*/
  setWindowsStatus();
}

AddUserNameCardDialog::~AddUserNameCardDialog() { delete ui; }

void AddUserNameCardDialog::setDialogInvalid(bool status) {
  if (status) {
    ui->invalid->hide();
    ui->valid->show();
  } else {
    ui->valid->hide();
    ui->invalid->show();
  }
}

void AddUserNameCardDialog::setupUserInfo(std::unique_ptr<UserNameCard> info) {
  m_info = std::move(info);
  ui->user_name->setText(m_info->m_nickname);

  /*load image resources*/
  // Tools::loadImgResources({m_info->m_avatorPath}, ui->user_avator->width(),
  //                         ui->user_avator->height(), "/static/");

  // Tools::setQLableImage(ui->user_avator, m_info->m_avatorPath, "/static/");

    Tools::loadAvatarResources(std::make_shared<UserNameCard>(*info),  ui->user_avator);
}

void AddUserNameCardDialog::setWindowsStatus() {
  setWindowTitle("Add friend");
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

  /*set it to modal*/
  setModal(true);
}

void AddUserNameCardDialog::registerSignal() {}

void AddUserNameCardDialog::on_add_friend_button_clicked() {
  qDebug() << "[Valid Mode]: Add friend button clicked!";

  /*hide current AddUserNameCardDialog*/
  this->hide();

  /*display AddUserRequestDialog UI*/
  AddUserRequestDialog *dialog(new AddUserRequestDialog(this));
  dialog->setModal(true);
  dialog->show();
}

void AddUserNameCardDialog::on_close_clicked() {
  qDebug() << "[Invalid Mode]: Add friend button clicked!";

  /*hide current AddUserNameCardDialog*/
  this->hide();
}
