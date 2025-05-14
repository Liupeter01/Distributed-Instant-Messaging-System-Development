#ifndef IMAGECROPPERDIALOG_H
#define IMAGECROPPERDIALOG_H

#include <QDialog>
#include <imagecropperqlabel.h>

namespace Ui {
class ImageCropperDialog;
}

class ImageCropperDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageCropperDialog(const std::size_t _width,
                                const std::size_t _height,
                                QWidget *parent = nullptr);

    ~ImageCropperDialog();

private slots:
    void on_ok_clicked();
    void on_cancel_clicked();

protected:
    const QSize getQLabelSize() const;

private:
    /*windows size setting*/
    std::size_t m_width;
    std::size_t m_height;
    Ui::ImageCropperDialog *ui;
};

#endif // IMAGECROPPERDIALOG_H
