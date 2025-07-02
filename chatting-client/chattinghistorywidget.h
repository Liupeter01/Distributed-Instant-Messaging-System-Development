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
  void updateLastMsg();
  void setItemDisplay();

  std::shared_ptr<UserNameCard> getFriendsInfo();

private:
  // template <typename _Type, check_datatype_v<_Type> = 0> void setLastMessage() {
  //   auto &target = std::get<_Type>(*m_userinfo->getChattingHistory());
  //   /*if it is empty*/
  //   if (!target.m_data.size()) {
  //     ui->last_message->setText("");
  //     return;
  //   }

  //   if constexpr (std::is_same_v<ChattingTextMsg, std::decay_t<_Type>>) {
  //     ui->last_message->setText((*(target.m_data.end() - 1))->m_msg_content);
  //   } else if constexpr (std::is_same_v<ChattingVoice, std::decay_t<_Type>>) {

  //   } else if constexpr (std::is_same_v<ChattingVideo, std::decay_t<_Type>>) {
  //   }
  // }

private:
  Ui::ChattingHistoryWidget *ui;
  std::shared_ptr<UserNameCard> m_userinfo;
};

#endif // CHATTINGHISTORYWIDGET_H
