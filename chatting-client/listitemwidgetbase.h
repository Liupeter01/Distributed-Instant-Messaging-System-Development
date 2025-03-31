#ifndef LISTITEMWIDGETBASE_H
#define LISTITEMWIDGETBASE_H

#include <QWidget>

enum class ListItemType {
  Default,            // current nothing being set
  ChattingHistory,    // display on chatting list
  MyContact,          // display on contact list
  ShowContactByGroup, // seperate contact by group
  SearchUserId,       // search user clickable widget
  AddUserWidget,      // show a add user widget on contact list
  NoUserFound         // server return no user found
};

class ListItemWidgetBase : public QWidget {
  Q_OBJECT
public:
  explicit ListItemWidgetBase(QWidget *parent = nullptr);
  virtual ~ListItemWidgetBase();

public:
  const ListItemType getItemType() const;
  virtual QSize sizeHint() const;

  static constexpr std::size_t width = 250;
  static constexpr std::size_t height = 70;

protected:
  void setItemType(ListItemType type);

private:
  ListItemType m_type;
};

#endif // LISTITEMWIDGETBASE_H
