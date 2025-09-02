#ifndef ADDUSERNAMECARDDIALOG_H
#define ADDUSERNAMECARDDIALOG_H

/*store user info*/
#include <QDialog>
#include <UserDef.hpp>

namespace Ui {
class AddUserNameCardDialog;
}

/* forward declartion */
class AddUserRequestDialog;
class AuthenticateNewFriendRequestDialog;

class AddUserNameCardDialog : public QDialog {
  Q_OBJECT

  friend class AddUserRequestDialog;
  friend class AuthenticateNewFriendRequestDialog;

public:
  explicit AddUserNameCardDialog(QWidget *parent = nullptr);
  virtual ~AddUserNameCardDialog();

public:
  /*
   * show invalid window or valid
   * valid = when signal_username_search returns a correct result
   * invalid = oppsite from valid
   */
  void setDialogInvalid(bool status);

  /*set usernamecard*/
  void setupUserInfo(std::unique_ptr<UserNameCard> info);

private slots:
  void on_add_friend_button_clicked();
  void on_close_clicked();

private:
  /*register signal*/
  void registerSignal();

  void setWindowsStatus();

private:
  Ui::AddUserNameCardDialog *ui;

  /*store user info*/
  std::unique_ptr<UserNameCard> m_info;
};

#endif // ADDUSERNAMECARDDIALOG_H
