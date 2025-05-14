#include "imagecropperdialog.h"
#include "ui_imagecropperdialog.h"

ImageCropperDialog::ImageCropperDialog(const std::size_t _width, const std::size_t _height, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImageCropperDialog)
    , m_width(_width)
    , m_height(_height)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle("Image Cropper");
    this->setModal(true);
    this->setMouseTracking(true);

    this->setMinimumSize(m_width, m_height);
}

ImageCropperDialog::~ImageCropperDialog()
{
    delete ui;
}

void ImageCropperDialog::on_ok_clicked(){

}

void ImageCropperDialog::on_cancel_clicked(){

}

const QSize ImageCropperDialog::getQLabelSize() const
{
    const auto box_size = ui->choose_box->size();
    return QSize(m_width - box_size.width(), m_height- box_size.height());
}
