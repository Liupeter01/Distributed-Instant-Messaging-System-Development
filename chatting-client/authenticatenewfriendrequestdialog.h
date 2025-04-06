#ifndef AUTHENTICATENEWFRIENDREQUESTDIALOG_H
#define AUTHENTICATENEWFRIENDREQUESTDIALOG_H

#include <QDialog>
#include <QString>
#include <map>
#include <memory>
#include <vector>
/*store user info*/
#include "UserNameCard.h"
#include "onceclickableqlabel.h"
#include "usertagwidget.h"
#include <MsgNode.hpp>
#include <ByteOrderConverter.hpp>

namespace Ui {
class AuthenticateNewFriendRequestDialog;
}

class AuthenticateNewFriendRequestDialog : public QDialog {
  Q_OBJECT

    using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;

public:
  explicit AuthenticateNewFriendRequestDialog(QWidget *parent = nullptr);
  virtual ~AuthenticateNewFriendRequestDialog();

public:
  /*
   * transfer UserNameCard structure to current class
   * We need to send request by using this stucture
   */
  void setUserInfo(std::unique_ptr<UserNameCard> card);

private:
  /*register signal<->slot*/
  void registerSignal();

  /*set up placeholder text*/
  void setupDefaultInfo();

  /*set up ui style*/
  void setupWindowStyle();

  /*customlized functions*/
  bool eventFilter(QObject *object, QEvent *event) override;

  /*add new tag to the list*/
  void addNewTag2Container(const QString &text);

  /*add new tag input tag widget*/
  void addNewTag2InputTag(const QString &text);

  /*add new tag to exisiting existing_tag_widget*/
  void addNewTag2ExistingTag(const QString &text);

  /*creat selected or existing tag by using smart pointer*/
  void createSelectedTag(const QString &text, UserTagWidget *widget);
  void createExistingTag(const QString &text, OnceClickableQLabel *widget);

  /*reset labels*/
  void resetLabels();

  /*shutdown AddUserRequestDialog*/
  void closeDialog();

  /*test function*/
  void loadtestFunction();

private slots:
  /*click confirm*/
  void on_confirm_button_clicked();

  /*click cancel*/
  void on_cancel_button_clicked();

  /*connect with show_more_label clickable qlabel*/
  void slot_show_more_label();

  /*user press enter at tag_input*/
  void slot_input_tag_press_enter();

  /*user press close icon on qlabel*/
  void slot_remove_selected_tag();

  /*user input text in tag_input, bind with textchange signal*/
  void slot_input_tag_textchange(const QString &text);

  /*user finished input text in tag_input*/
  void slot_input_tag_finished();

  /*1.user choose a tag from existing tag, and add it to selected_label
   *2.user click the user_tag_display_bar, and add it to both selected &
   *existing label
   */
  void slot_choose_tag_by_click(QString str, LabelState state);

private:
  Ui::AuthenticateNewFriendRequestDialog *ui;

  /*prefix string*/
  const QString prefix_string;

  /*using a parameter to adjust height for user experience*/
  static constexpr std::size_t COMPENSATION_HEIGHT = 4;
  static constexpr std::size_t COMPENSATION_WIDTH = 2;

  /*
   * record the position of the existing label(existing_label)
   * record the position of the selected label(tag_label)
   */
  QPoint m_existing_cur_pos;
  QPoint m_selected_cur_pos;

  /*store all tags*/
  std::vector<QString> m_tagLists;

  /*labels that were already created! might store in server & local database*/
  std::vector<QString> m_existing_key;
  std::map<QString, std::shared_ptr<OnceClickableQLabel>> m_exist_label;

  /*update existing ui display*/

  /*labels that are going to be added to textedit widget*/
  std::vector<QString> m_selected_key;
  std::map<QString, std::shared_ptr<UserTagWidget>> m_selected_label;

  /*store user info, data comes from addusernamecardwidget class*/
  std::unique_ptr<UserNameCard> m_info;
};

#endif // AUTHENTICATENEWFRIENDREQUESTDIALOG_H
