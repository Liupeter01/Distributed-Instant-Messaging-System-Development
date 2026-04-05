#include "usersettingstackpage.h"
#include "ui_usersettingstackpage.h"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <imagecropperdialog.h>
#include <logicmethod.h>
#include <useraccountmanager.hpp>

UserSettingStackPage::UserSettingStackPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::UserSettingStackPage) {
  ui->setupUi(this);

  registerSignal();
}

UserSettingStackPage::~UserSettingStackPage() { delete ui; }

void UserSettingStackPage::registerSignal() {

  connect(this, &UserSettingStackPage::signal_start_file_transmission,
          LogicMethod::get_instance().get(),
          &LogicMethod::signal_start_file_transmission);
}

void UserSettingStackPage::on_submit_clicked() {
  if (m_fileName.isEmpty() || m_filePath.isEmpty()) {
    qDebug() << "No Valid Avator file Selected!\n";
    return;
  }

  // reset pause status to prevent unexpected error(because we do not need file
  // upload UI here!)
  LogicMethod::get_instance()->setPause(false);

  // start to transmit avator to resources server
  emit signal_start_file_transmission(m_fileName, m_filePath);
}

void UserSettingStackPage::on_select_avator_clicked() {

  QString filepath = QFileDialog::getOpenFileName(
      this, tr("Choose Your Avator"), QString{},
      tr("Image Format(*.png *.jpg *.jpeg *.bmp *.webp)"));

  if (filepath.isEmpty()) {
    QMessageBox::critical(this, tr("Error!"), tr("No Image Selected!"),
                          QMessageBox::Ok);
    return;
  }

  auto image = ImageCropperDialog::getCroppedImage(filepath, 600, 400,
                                                   CroppingShape::CIRCLE);
  if (image.isNull()) {
    qDebug() << "image cropped error!\n";
    QMessageBox::critical(this, tr("Error!"), tr("Image Processing Error!"),
                          QMessageBox::Ok);
    return;
  }

  // scale the pixmap to suit new_avator(QLabel) size!
  m_avator = image.scaled(ui->new_avator->size(), Qt::KeepAspectRatio,
                          Qt::SmoothTransformation);

  ui->new_avator->setPixmap(m_avator);     // display this image on qlabel
  ui->new_avator->setScaledContents(true); // scale automatically!

  // Create Sub dir
  QString appDir = QCoreApplication::applicationDirPath();
  QString subDirName = "avatars";
  QDir dir(appDir);

  if (!dir.exists(subDirName)) {
      if (!dir.mkdir(subDirName)) {
          qDebug() << "Create Directory Failed:" << dir.absoluteFilePath(subDirName);
          QMessageBox::critical(this, tr("Error"), tr("Check your Privilege"));
          return;
      }
  }

  dir.cd(subDirName);

  m_fileName = QString("avatar_%1.png").arg(UserAccountManager::get_instance()->get_uuid());

  m_filePath = dir.filePath(m_fileName);

  if (!m_avator.save(m_filePath, "PNG")) {
      qDebug() << "Save failed to:" << m_filePath;
      QMessageBox::critical(this, tr("Save Error"), tr("Check your Privilege"));
      return;
  }

  qDebug() << "Avatar Has Been Storged to path = " << m_filePath << "\n";
}
