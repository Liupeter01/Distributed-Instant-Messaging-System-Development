#include "imagecropperdialog.h"
#include <QMessageBox>

QPixmap ImageCropperDialog::getCroppedImage(const QString &filename,
                                            int windowWidth, int windowHeight,
                                            CroppingShape cropperShape) {
  QPixmap inputImage;
  QPixmap outputImage;

  if (!inputImage.load(filename)) {
    QMessageBox::critical(nullptr, "Error", "Load image failed!",
                          QMessageBox::Ok);
    return outputImage;
  }

  ImageCropperDialogImpl *impl = new ImageCropperDialogImpl(
      windowWidth, windowHeight, inputImage, outputImage, cropperShape);

  impl->exec();

  return outputImage;
}
