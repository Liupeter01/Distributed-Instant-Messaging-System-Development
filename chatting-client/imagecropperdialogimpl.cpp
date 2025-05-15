#include "imagecropperdialogimpl.h"
#include "ui_imagecropperdialogimpl.h"

ImageCropperDialogImpl::ImageCropperDialogImpl(const std::size_t _width,
                                       const std::size_t _height,
                                       const QPixmap& original_image,
                                       QPixmap &output_image,
                                       const CroppingShape &shape,
                                       QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImageCropperDialogImpl)
    , m_width(_width)
    , m_height(_height)
    , m_outputPixmap(output_image)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle("Image Cropper");
    this->setModal(true);
    this->setMouseTracking(true);
    this->setMinimumSize(m_width, m_height);

    ui->imageLabel->setCropper(CroppingShape::CIRCLE, QSize(m_width, m_height));
    setOriginalPixmap(original_image);

    //in order to ganratee the safty, we should assign input image to output
    output_image = original_image;
}

ImageCropperDialogImpl::~ImageCropperDialogImpl(){
    delete ui;
}

void ImageCropperDialogImpl::on_ok_clicked(){
    auto image = ui->imageLabel->getCropppedImage();
    if(image.has_value()){
        m_outputPixmap = image.value();
    }
    this->close();
}

void ImageCropperDialogImpl::on_cancel_clicked(){
    ui->imageLabel->resetCropper();
}

void ImageCropperDialogImpl::setOriginalPixmap(const QPixmap &image){
    ui->imageLabel->setOriginalPixmap(image);
}
