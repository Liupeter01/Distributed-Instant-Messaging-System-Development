#ifndef IMAGECROPPERDIALOGIMPL_H
#define IMAGECROPPERDIALOGIMPL_H

#include <QDialog>
#include <QPixmap>
#include <imagecropperqlabel.h>

namespace Ui {
class ImageCropperDialogImpl;
}

class ImageCropperDialogImpl : public QDialog
{
    Q_OBJECT

public:
    explicit ImageCropperDialogImpl(const std::size_t _width,
                                const std::size_t _height,
                                const QPixmap& original_image,
                                QPixmap &output_image,
                                const CroppingShape& shape = CroppingShape::CIRCLE,
                                QWidget *parent = nullptr);

    virtual ~ImageCropperDialogImpl();

public:
    void setOriginalPixmap(const QPixmap& image);

private slots:
    void on_ok_clicked();
    void on_cancel_clicked();

protected:
private:
    /*windows size setting*/
    std::size_t m_width;
    std::size_t m_height;
    Ui::ImageCropperDialogImpl *ui;

    QPixmap m_originalPixmap;
    QPixmap& m_outputPixmap;
};

#endif // IMAGECROPPERDIALOGIMPL_H
