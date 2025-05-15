#include "usersettingstackpage.h"
#include "ui_usersettingstackpage.h"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <imagecropperdialog.h>

UserSettingStackPage::UserSettingStackPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::UserSettingStackPage) {
  ui->setupUi(this);
}

UserSettingStackPage::~UserSettingStackPage() { delete ui; }

void UserSettingStackPage::on_submit_clicked() {}

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
}
