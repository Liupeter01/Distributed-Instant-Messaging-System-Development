#ifndef TOOLS_H
#define TOOLS_H

#include <QDebug>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <initializer_list>
#include <map>
#include <optional>
#include <type_traits>

struct LabelState {
  LabelState();

  /*
   * Default status = DISABLE
   * VisiableStatus will be changed after every clicked!
   */
  enum VisiableStatus { DISABLED, ENABLED } visiable;

  enum class HoverStatus : uint8_t { DISABLED, ENABLED } hover;
};

struct PushButtonState {
  PushButtonState();

  /*
   * Default status = DISABLE
   * VisiableStatus will be changed after every clicked!
   */
  enum SelectedStatus { DISABLED, ENABLED } select;

  enum class HoverStatus : uint8_t { DISABLED, ENABLED } hover;
};

template <typename Widget, class = void>
struct has_settext_function : std::false_type {};

template <typename Widget>
struct has_settext_function<
    Widget, std::void_t<decltype(std::declval<std::decay_t<Widget>>().setText(
                QString()))>> : std::true_type {};

struct Tools {
  static void refreshQssStyle(QWidget *widget);

  template <typename Widget,
            typename std::enable_if<
                has_settext_function<std::remove_pointer_t<Widget>>::value,
                int>::type = 0>
  static void setWidgetAttribute(Widget widget, const QString &message,
                                 bool status) {
    widget->setText(message);
    widget->setProperty("state", (status ? "correct" : "incorrect"));
    refreshQssStyle(widget);
  }

  /*handling url info*/
  static void readConfigrationFile(QString file_name = QT_DEMO_HOME
                                   "config.ini");
  static QUrl getTargetUrl(QString param = ""); // for public access
  static QString url_info;                      // store url info
  static bool url_init_flag;                    // url is init or not?

  /*check validation*/
  static bool checkUsername(QLineEdit *edit, QLabel *label);
  static bool checkEmail(QLineEdit *edit, QLabel *label);
  static bool checkPassword(QLineEdit *edit, QLabel *label);
  static bool checkSimilarity(QLineEdit *edit_pass, QLineEdit *edit_confirm,
                              QLabel *label);
  static bool checkCaptcha(QLineEdit *edit, QLabel *label);

  /*load image*/
  static std::map<QString, QImage> s_images;

  /*
   * all program needed images files should be inside "/res/" dir
   * all other user owned images should be loaded from "/static/" dir
   */
  static void loadImgResources(std::initializer_list<QString> file_list,
                               int width, int height,
                               const QString &load_dir = "/res/");

  static void setQLableImage(QLabel *label, const QString &target,
                             const QString &load_dir = "/res/");
  static std::optional<QImage> loadImages(const QString &path, int width,
                                          int height);

  /*load icon for chatting main frame*/
  static std::map<QString, QIcon> s_icons;

  static std::optional<QIcon> loadIcon(const QString &path);
  static void setPushButtonIcon(QPushButton *button, const QString &target,
                                const QString &load_dir = "/res/");

  /*
   * all program needed images files should be inside "/res/" dir
   * all other user owned images should be loaded from "/static/" dir
   */
  static void loadIconResources(std::initializer_list<QString> file_list,
                                const QString &load_dir = "/res/");
};

Q_DECLARE_METATYPE(LabelState)
Q_DECLARE_METATYPE(PushButtonState)

#endif // TOOLS_H
