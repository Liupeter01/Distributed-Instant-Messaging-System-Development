#include "tools.h"
#include <QApplication>
#include <QDir>
#include <QObject>
#include <QRegularExpression>
#include <QSettings> /*handle ini file*/
#include <QString>
#include <QStyle>

QString Tools::url_info{};
bool Tools::url_init_flag{false};

std::map<QString, QImage> Tools::s_images{};
std::map<QString, QIcon> Tools::s_icons{};

void Tools::refreshQssStyle(QWidget *widget) {
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
}

void Tools::readConfigrationFile(QString file_name) {
  /*configuration file and using separator and toNativeSeparators*/
  QSettings setting(file_name, QSettings::IniFormat);

  /*generate Url*/
  auto host = setting.value("ServerSetting/host").toString();
  auto port = setting.value("ServerSetting/port").toUInt();

  qDebug() << "host = " << host << ", port = " << port << '\n';
  url_info = QString("http://") + host + QString(":") + QString::number(port);

  /*init success then set flag to true*/
  url_init_flag = true;
}

QUrl Tools::getTargetUrl(QString param) {
  if (!url_init_flag) {
    Tools::readConfigrationFile(/*config.ini*/);
  }
  return QUrl(url_info + param);
}

bool Tools::checkUsername(QLineEdit *edit, QLabel *label) { return true; }

bool Tools::checkEmail(QLineEdit *edit, QLabel *label) {
  QRegularExpression reg_email(
      QObject::tr("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+.[a-zA-Z]{2,}$"));
  if (!reg_email.match(edit->text()).hasMatch()) {
    Tools::setWidgetAttribute(label, QString("Invalid E-mail address"), false);
    return false;
  }
  Tools::setWidgetAttribute(label, QString("E-mail Check Passed!"), true);
  return true;
}

bool Tools::checkPassword(QLineEdit *edit, QLabel *label) {
  /* at least one capital letter(A-Z)
   * at least one lower case letter(a-z)
   * at least one number(0-z)
   * at least 8 character
   */
  QRegularExpression reg_password(
      QObject::tr("(?=.*[A-Z])(?=.*[a-z])(?=.*[0-9]).{8,}"));

  /*does not match the regular expression critiera!*/
  if (!reg_password.match(edit->text()).hasMatch()) {
    Tools::setWidgetAttribute(label, QString("Invalid Password!"), false);
    return false;
  }
  Tools::setWidgetAttribute(label, QString("Password Check Passed!"), true);
  return true;
}

bool Tools::checkSimilarity(QLineEdit *edit_pass, QLineEdit *edit_confirm,
                            QLabel *label) {
  /*does not match origin password*/
  if (!checkPassword(edit_pass, label)) {
    return false;
  }

  if (edit_pass->text() != edit_confirm->text()) {
    Tools::setWidgetAttribute(label, QString("Password does not matched!"),
                              false);
    return false;
  }
  Tools::setWidgetAttribute(label, QString("Password matched!"), true);
  return true;
}

bool Tools::checkCaptcha(QLineEdit *edit, QLabel *label) {
  if (edit->text().isEmpty()) {
    Tools::setWidgetAttribute(label, QString("CAPTCHA can not be empty!"),
                              false);
    return false;
  }
  return true;
}

std::optional<QImage> Tools::loadImages(const QString &path, int width,
                                        int height) {
  QFile load_file(path);
  if (!load_file.exists()) {
    qDebug() << "Open image path: " << path << " failed!";
    return std::nullopt;
  }

  QImage load_img(path);
  load_img = load_img.scaled(width, height);
  return load_img;
}

std::optional<QIcon> Tools::loadIcon(const QString &path) {
  QFile load_file(path);
  if (!load_file.exists()) {
    qDebug() << "Open image path: " << path << " failed!";
    return std::nullopt;
  }
  return QIcon(path);
}

void Tools::loadImgResources(std::initializer_list<QString> file_list,
                             int width, int height, const QString &load_dir) {
  for (const auto &path : file_list) {

    /*we do not allow duplication in map*/
    if (Tools::s_images.find(path) == Tools::s_images.end()) {
      auto image =
          Tools::loadImages(QT_DEMO_HOME + load_dir + path, width, height);

      if (!image.has_value()) {
        qDebug() << "image: " << path << " load error!";
        continue;
      }
      qDebug() << "resource image file: " << path << " load successfully!";
      Tools::s_images.insert(
          std::pair<QString, QImage>(load_dir + path, image.value()));
    }
  }
}

void Tools::loadIconResources(std::initializer_list<QString> file_list,
                              const QString &load_dir) {
  for (const auto &path : file_list) {

    auto image = loadIcon(QT_DEMO_HOME + load_dir + path);

    if (!image.has_value()) {
      qDebug() << "qicon: " << path << " load error!";
      continue;
    }
    qDebug() << "resource icon file: " << path << " load successfully!";
    Tools::s_icons.insert(
        std::pair<QString, QIcon>(load_dir + path, image.value()));
  }
}

void Tools::setQLableImage(QLabel *label, const QString &target,
                           const QString &load_dir) {
  auto it = Tools::s_images.find(load_dir + target);
  if (it == Tools::s_images.end()) {
    qDebug() << "image: " << target << " not found!";
    return;
  }
  label->setPixmap(QPixmap::fromImage(it->second));
  label->setScaledContents(true);
  label->update();
}

void Tools::setPushButtonIcon(QPushButton *button, const QString &target,
                              const QString &load_dir) {
  auto it = Tools::s_icons.find(load_dir + target);
  if (it == Tools::s_icons.end()) {
    qDebug() << "icon: " << target << " not found!";
    return;
  }
  button->setIcon(it->second);
  button->setIconSize(button->size());
  button->update();
}

LabelState::LabelState()
    : visiable(VisiableStatus::DISABLED), hover(HoverStatus::DISABLED) {}

PushButtonState::PushButtonState()
    : select(SelectedStatus::DISABLED), hover(HoverStatus::DISABLED) {}
