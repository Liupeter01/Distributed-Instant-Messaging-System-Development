#ifndef IMAGECROPPERDIALOG_H
#define IMAGECROPPERDIALOG_H

#include <imagecropperdialogimpl.h>

struct ImageCropperDialog{
public:
    [[nodiscard]] static QPixmap getCroppedImage(const QString& filename,
                                   int windowWidth,
                                   int windowHeight,
                                   CroppingShape cropperShape);

};

#endif // IMAGECROPPERDIALOG_H
