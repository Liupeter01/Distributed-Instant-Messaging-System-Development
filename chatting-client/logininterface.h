#ifndef LOGININTERFACE_H
#define LOGININTERFACE_H

#include "httpnetworkconnection.h"
#include "tcpnetworkconnection.h"
#include "tools.h"
#include <MsgNode.hpp>
#include <QDialog>
#include <functional>
#include <map>

/*declaration of network events*/
enum class ServiceType : uint8_t;
enum class ServiceStatus : uint8_t;

namespace Ui {
class LoginInterface;
}

class LoginInterface : public QDialog {
  Q_OBJECT

  using CallBackFunc = std::function<void(QJsonObject &&json)>;
  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;

public:
  explicit LoginInterface(QWidget *parent = nullptr);
  virtual ~LoginInterface();

private:
  void registerSignal();
  void setLoginAttribute();
  void registerNetworkEvent();
  void regisrerCallBackFunctions();
  void slot_forgot_passwd();
  void slot_login_finished(ServiceType srv_type, QString json_data,
                           ServiceStatus srv_status);

  /*display/hide password button clicked*/
  void handle_clicked();

  /*mouse enter/leave qimage area*/
  void handle_hover();

signals:
  /*switch to register interface*/
  void switchWindow();

  /*switch to reset interface*/
  void switchReset();

  /*try to connect to chatting server*/
  void signal_connect2_chatting_server();

private slots:
  void on_login_button_clicked();

  /*connection status, indicator*/
  void slot_connection_status(bool status);

private:
  Ui::LoginInterface *ui;
  std::map<ServiceType, CallBackFunc> m_callbacks;
};

#endif // LOGININTERFACE_H
