#ifndef CHATTINGHISTORYWIDGET_H
#define CHATTINGHISTORYWIDGET_H

#include "listitemwidgetbase.h"
#include "ui_chattinghistorywidget.h"
#include <UserDef.hpp>

enum class MsgType;

class ChattingHistoryWidget : public ListItemWidgetBase {
  Q_OBJECT

public:
  ChattingHistoryWidget(QWidget *parent = nullptr);
  virtual ~ChattingHistoryWidget();

  void setUserInfo(std::shared_ptr<UserNameCard> info);
  void setLastMessage(const QString &msg);
  void setItemDisplay();

  std::shared_ptr<UserNameCard> getFriendsInfo();

private:
  Ui::ChattingHistoryWidget *ui;
  std::shared_ptr<UserNameCard> m_userinfo;
};

#endif // CHATTINGHISTORYWIDGET_H
