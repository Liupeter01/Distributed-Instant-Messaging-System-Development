#ifndef ADDUSERNAMECARDWIDGET_H
#define ADDUSERNAMECARDWIDGET_H

#include <QFrame>
#include <UserDef.hpp>
#include <memory>

namespace Ui {
class AddUserNameCardWidget;
}

class AddUserNameCardWidget : public QFrame {
  Q_OBJECT

public:
  explicit AddUserNameCardWidget(QWidget *parent = nullptr);
  virtual ~AddUserNameCardWidget();

public:
  // void setFriendAvator(const QPixmap &pic);
  void setNameCardInfo(std::shared_ptr<UserFriendRequest> info);

  static const QSize getImageSize();
  virtual QSize sizeHint() const;

  static constexpr std::size_t width = 270;
  static constexpr std::size_t height = 64;

  static constexpr std::size_t image_width = 40;
  static constexpr std::size_t image_height = 40;

private:
  /*register signal*/
  void registerSignal();

  /*default settings*/
  void loadDefaultSetting();

signals:
  void signal_add_friend(std::shared_ptr<UserFriendRequest>);

private slots:

private:
  Ui::AddUserNameCardWidget *ui;
  std::shared_ptr<UserFriendRequest> m_info;
};

#endif // ADDUSERNAMECARDWIDGET_H
