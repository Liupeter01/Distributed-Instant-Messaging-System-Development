#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtEndian>

#include "logininterface.h"
#include "ui_logininterface.h"
#include "useraccountmanager.hpp"

LoginInterface::LoginInterface(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginInterface) {
  ui->setupUi(this);

  /*register pushbutton signal for page swiping*/
  registerSignal();

  /*set login password*/
  setLoginAttribute();

  registerNetworkEvent();
  regisrerCallBackFunctions();

  /*load registeration interface's image*/
  Tools::loadImgResources(
      {"show_password.png", "show_passwd_selected.png",
       "invisiable_password.png", "invisiable_passwd_selected.png"},
      ui->passwd_display->width(), ui->passwd_display->height());

  /*set default image for registeration page*/
  Tools::setQLableImage(ui->passwd_display, "invisiable_password.png");
}

LoginInterface::~LoginInterface() { delete ui; }

void LoginInterface::registerSignal() {
  connect(this->ui->register_button, &QPushButton::clicked, this,
          &LoginInterface::switchWindow);
  connect(this->ui->forgot_passwd_label, &ForgotPassword::clicked, this,
          &LoginInterface::slot_forgot_passwd);

  connect(this->ui->forgot_passwd_label, &ForgotPassword::update_display, this,
          [this]() {
            auto state = ui->forgot_passwd_label->getState();
            if (state.hover == LabelState::HoverStatus::ENABLED) {
              Tools::setWidgetAttribute(ui->forgot_passwd_label,
                                        "forgot password?", true);
            } else {
              Tools::setWidgetAttribute(ui->forgot_passwd_label,
                                        "forgot password?", false);
            }
          });

  connect(this->ui->passwd_display, &MultiClickableQLabel::clicked, this,
          [this]() {
            handle_clicked();
            handle_hover();
          });
  connect(this->ui->passwd_display, &MultiClickableQLabel::update_display, this,
          [this]() {
            handle_clicked();
            handle_hover();
          });
}

void LoginInterface::setLoginAttribute() {
  /*set password editing attribute*/
  this->ui->passwd_edit->setEchoMode(QLineEdit::Password);
}

void LoginInterface::registerNetworkEvent() {
  connect(HttpNetworkConnection::get_instance().get(),
          &HttpNetworkConnection::signal_login_finished, this,
          &LoginInterface::slot_login_finished);

  connect(this, &LoginInterface::signal_connect2_chatting_server,
          TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_connect2_chatting_server);

  /*connect connection signal <--> slot */
  connect(TCPNetworkConnection::get_instance().get(),
          &TCPNetworkConnection::signal_connection_status, this,
          &LoginInterface::slot_connection_status);
}

void LoginInterface::regisrerCallBackFunctions() {
  m_callbacks.insert(std::pair<ServiceType, CallBackFunc>(
      ServiceType::SERVICE_LOGINDISPATCH, [this](QJsonObject &&json) {
        auto error = json["error"].toInt();

        if (error != static_cast<uint8_t>(ServiceStatus::SERVICE_SUCCESS)) {
          Tools::setWidgetAttribute(this->ui->status_label_3,
                                    QString("Service Error!"), false);

          /*restore button input*/
          ui->login_button->setEnabled(true);
          return;
        }

        Tools::setWidgetAttribute(this->ui->status_label_3,
                                  QString("Login Success!"), true);

        UserAccountManager::get_instance()->set_uuid(json["uuid"].toString());
        UserAccountManager::get_instance()->set_host(json["host"].toString());
        UserAccountManager::get_instance()->set_port(json["port"].toString());
        UserAccountManager::get_instance()->set_token(json["token"].toString());

        emit signal_connect2_chatting_server();
      }));
}

void LoginInterface::slot_login_finished(ServiceType srv_type,
                                         QString json_data,
                                         ServiceStatus srv_status) {
  /*handle network error*/
  if (!json_data.length() && srv_status == ServiceStatus::NETWORK_ERROR) {
    Tools::setWidgetAttribute(this->ui->status_label_3,
                              QString("Network Error!"), false);

    /*restore button input*/
    ui->login_button->setEnabled(true);
    return;
  }

  // json_data
  QJsonDocument json_obj = QJsonDocument::fromJson(json_data.toUtf8());
  if (json_obj.isNull()) { // converting failed
    Tools::setWidgetAttribute(this->ui->status_label_3,
                              QString("Retrieve Data Error!"), false);
    // journal log system
    qDebug() << "[FATAL ERROR]: json object is null!\n";

    /*restore button input*/
    ui->login_button->setEnabled(true);
    return;
  }

  if (!json_obj.isObject()) {
    Tools::setWidgetAttribute(this->ui->status_label_3,
                              QString("Retrieve Data Error!"), false);
    // journal log system
    qDebug() << "[FATAL ERROR]: json can not be converted to an object!\n";

    /*restore button input*/
    ui->login_button->setEnabled(true);
    return;
  }

  /*to prevent app crash due to callback is not exists*/
  try {
    m_callbacks[srv_type](std::move(json_obj.object()));
  } catch (const std::exception &e) {
    qDebug() << e.what();
  }
}

void LoginInterface::handle_clicked() {
  auto click = [this](MultiClickableQLabel *label, QLineEdit *edit) {
    auto state = label->getState();
    if (state.visiable == LabelState::VisiableStatus::ENABLED) {
      edit->setEchoMode(QLineEdit::Normal);
      Tools::setQLableImage(label, "show_password.png");
    } else {
      edit->setEchoMode(QLineEdit::Password);
      Tools::setQLableImage(label, "invisiable_password.png");
    }
  };

  click(ui->passwd_display, ui->passwd_edit);
}

void LoginInterface::handle_hover() {
  auto hover = [this](MultiClickableQLabel *label) {
    auto state = label->getState();
    if (state.hover == LabelState::HoverStatus::ENABLED) {
      Tools::setQLableImage(label, state.visiable
                                       ? "show_passwd_selected.png"
                                       : "invisiable_passwd_selected.png");
    } else {
      Tools::setQLableImage(label, state.visiable ? "show_password.png"
                                                  : "invisiable_password.png");
    }
  };

  hover(ui->passwd_display);
}

void LoginInterface::slot_forgot_passwd() {
  emit switchReset();
  return;
}

void LoginInterface::on_login_button_clicked() {
  QString username = this->ui->username_edit->text();
  QString passwd = this->ui->passwd_edit->text();

  QJsonObject json;
  json["username"] = username;
  json["password"] = passwd;

  HttpNetworkConnection::get_instance()->postHttpRequest(
      Tools::getTargetUrl("/trylogin_server"), json,
      ServiceType::SERVICE_LOGINDISPATCH);

  /*prevent user click the button so many times*/
  ui->login_button->setEnabled(false);
}

void LoginInterface::slot_connection_status(bool status) {
  if (status) {
    Tools::setWidgetAttribute(ui->status_label_3,
                              QString("Connection Established, Connecting..."),
                              true);

    QJsonObject json_obj;
    json_obj["uuid"] = UserAccountManager::get_instance()->get_uuid();
    json_obj["token"] = UserAccountManager::get_instance()->get_token();

    /*after connection to server, send TCP request*/
    TCPNetworkConnection::send_buffer(ServiceType::SERVICE_LOGINSERVER,
                                      std::move(json_obj)
     );

  } else {
    Tools::setWidgetAttribute(ui->status_label_3, QString("Network error!"),
                              false);

    /*restore button input*/
    ui->login_button->setEnabled(true);
  }
}
