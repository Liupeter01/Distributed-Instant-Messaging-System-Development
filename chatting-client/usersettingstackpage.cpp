#include <QString>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QDebug>
#include "usersettingstackpage.h"
#include "ui_usersettingstackpage.h"

UserSettingStackPage::UserSettingStackPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UserSettingStackPage)
{
    ui->setupUi(this);
}

UserSettingStackPage::~UserSettingStackPage()
{
    delete ui;
}

void UserSettingStackPage::on_upload_avator_clicked(){
    QString filepath = QFileDialog::getOpenFileName(this,
                                 tr("Choose Your Avator"), QString{},
                                 tr("Image Format(*.png *.jpg *.jpeg *.bmp *.webp)"));

    if(filepath.isEmpty()){
        QMessageBox::critical(this, tr("Error!"), tr("No Image Selected!"), QMessageBox::Ok);
        return;
    }
    QPixmap image(filepath);
    if(image.isNull()){
        QMessageBox::critical(this, tr("Error!"), tr("Load Image Error!"), QMessageBox::Ok);
        return;
    }

    //scale the pixmap to suit new_avator(QLabel) size!
    image = image.scaled(ui->new_avator->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if(image.isNull()){
        qDebug() << "image scaled error!\n";
        QMessageBox::critical(this, tr("Error!"), tr("Image Processing Error!"), QMessageBox::Ok);
        return;
    }

    ui->new_avator->setPixmap(image);           //display this image on qlabel
    ui->new_avator->setScaledContents(true);    //scale automatically!

}

