#ifndef CHATTINGMSGITEM_H
#define CHATTINGMSGITEM_H
#include "msgbubblebase.h"
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QSpacerItem>
#include <QWidget>

enum class MessageStatus{
    UNREAD,
    READ,
    FAILED  //message sent error
};

/*
 * chattingmsgbubble is one of the component in ChattingMsgItem
 */
class ChattingMsgItem : public QWidget {
  Q_OBJECT

public:
  explicit ChattingMsgItem(ChattingRole role, QWidget *parent = nullptr);
  virtual ~ChattingMsgItem();

public:
  void setupUserName(const QString &name);
  void setupIconPixmap(const QPixmap &icon);
  void setupBubbleWidget(QWidget *bubble);
  void setupMsgStatus(const MessageStatus status);

  static constexpr std::size_t icon_width = 45;
  static constexpr std::size_t icon_height = 45;

  static constexpr std::size_t statusLabel_width = 15;
  static constexpr std::size_t statusLabel_height = 15;

private:
  void addStyleSheet();

private:
  QFont m_font;
  ChattingRole m_role;
  QLabel *m_nameLabel; /*display name*/
  QLabel *m_iconLabel; /*display user avator*/
  QLabel *m_statusLabel;    /*display if message is read or not?*/
  QGridLayout *m_grid; /*grid layout*/
  QSpacerItem *m_spacer;
  QWidget *m_bubble;
};

#endif // CHATTINGMSGITEM_H
