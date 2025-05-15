#include "imagecropperqlabel.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

ImageCropperQLabel::ImageCropperQLabel(QWidget *parent) : m_size(QSize{}) {

  this->setMouseTracking(true);
  this->setAlignment(Qt::AlignCenter);

  m_borderPen.setWidth(1);
  m_borderPen.setColor(Qt::white);
  m_borderPen.setDashPattern(QVector<qreal>() << 3 << 3 << 3 << 3);
}

ImageCropperQLabel::~ImageCropperQLabel() {}

void ImageCropperQLabel::setCropperSize(const QSize &size) {
  m_size = size;
  this->setFixedSize(m_size);
}

void ImageCropperQLabel::setCropperSize(const std::size_t _width,
                                        const std::size_t _height) {
  setCropperSize(QSize(_width, _height));
}

void ImageCropperQLabel::setCropperShape(const CroppingShape shape) {
  m_shape = shape;
}

void ImageCropperQLabel::setCropper(const CroppingShape shape,
                                    const QSize &size) {
  setCropperShape(shape);
  setCropperSize(size);
  m_originalCroppedRect.setSize(size);
}

void ImageCropperQLabel::setOriginalPixmap(const QPixmap &image) {
  if (image.isNull()) {
    return;
  }

  if (m_size.isEmpty() || !m_size.width() || !m_size.height()) {
    return;
  }

  m_originalImage = image;

  // calculating the aspect ratio to determine how to scale or position the
  // image
  auto label_width = this->m_size.width();
  auto label_height = this->m_size.height();
  auto image_width = m_originalImage.width();
  auto image_height = m_originalImage.height();

  auto label_ratio = label_height / static_cast<float>(label_width);

  int calibratedHeight{}, calibratedWidth{};

  if (label_ratio * image_width < image_height) {
    // it seems height is limiting dimension
    m_ratio = label_height / static_cast<float>(image_height);
    calibratedHeight = label_height;
    calibratedWidth = static_cast<int>(m_ratio * image_width);
    m_imageRect.setRect(
        static_cast<int>(
            label_width * 0.5f -
            calibratedWidth *
                0.5f), /*we need to find it's central point on width!*/
        0, calibratedWidth, calibratedHeight);
  } else {
    // it seems width is limiting dimension
    m_ratio = label_width / static_cast<float>(image_width);
    calibratedHeight = static_cast<int>(m_ratio * image_height);
    calibratedWidth = label_width;
    m_imageRect.setRect(
        0,
        static_cast<int>(
            label_height * 0.5f -
            calibratedHeight *
                0.5f), /*we need to find it's central point on height!*/
        calibratedWidth, calibratedHeight);
  }

  m_calibratedImage =
      m_originalImage.scaled(calibratedWidth, calibratedHeight,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);

  if (m_calibratedImage.isNull()) {
    qDebug() << "scale original image to the new one error!";
    return;
  }

  this->setPixmap(m_calibratedImage);

  // m_croppedRect.setWidth(static_cast<int>(m_originalCroppedRect.width() *
  // m_ratio));
  // m_croppedRect.setHeight(static_cast<int>(m_originalCroppedRect.height() *
  // m_ratio));

  // calculate new m_croppedRect according to m_shape
  resetCropper();
}

std::optional<QPixmap> ImageCropperQLabel::getCropppedImage() {
  return getCropppedImage(m_shape);
}

void ImageCropperQLabel::updateCursor(const CroppingPosition &pos) {

  switch (pos) {
  case CroppingPosition::OUTSIDE_RECT:
    setCursor(Qt::ArrowCursor);
    break;

  case CroppingPosition::BUTTOM_MID:
  case CroppingPosition::TOP_MID:
    setCursor(Qt::SizeVerCursor);
    break;

  case CroppingPosition::LEFT_MID:
  case CroppingPosition::RIGHT_MID:
    setCursor(Qt::SizeHorCursor);
    break;

  case CroppingPosition::LEFT_BUTTOM:
  case CroppingPosition::RIGHT_TOP:
    setCursor(Qt::SizeBDiagCursor);
    break;

  case CroppingPosition::RIGHT_BUTTOM:
  case CroppingPosition::LEFT_TOP:
    setCursor(Qt::SizeFDiagCursor);
    break;

  case CroppingPosition::INSIDE_RECT:
    setCursor(Qt::SizeAllCursor);
    break;

  default:
    break;
  }
}

std::optional<QPixmap>
ImageCropperQLabel::getCropppedImage(const CroppingShape shape) {
  /*
   * because of the image has already been transformed!
   * the transformed ratio is m_ratio
   * so all cropped values have to be modified according to this value
   */
  std::size_t start_x_pos = static_cast<std::size_t>(
      (m_croppedRect.left() - m_imageRect.left()) / m_ratio);
  std::size_t start_y_pos = static_cast<std::size_t>(
      (m_croppedRect.top() - m_imageRect.top()) / m_ratio);

  std::size_t cropped_width =
      static_cast<std::size_t>(m_croppedRect.width() / m_ratio);
  std::size_t cropped_height =
      static_cast<std::size_t>(m_croppedRect.height() / m_ratio);

  // ret value
  QPixmap res(cropped_width, cropped_height);
  // copy image from the original image
  res = m_originalImage.copy(start_x_pos, start_y_pos, cropped_width,
                             cropped_height);

  if (res.isNull()) {
    return std::nullopt;
  }

  // currently, we ignore this!
  if (shape == CroppingShape::ELLIPSE) {
  }
  return res;
}

void ImageCropperQLabel::paintEvent(QPaintEvent *e) {

  QLabel::paintEvent(e);

  switch (m_shape) {
  case CroppingShape::UNDEFINED:
    break;
  case CroppingShape::FIXED_RECT:
    drawRectOpacity();
    break;
  case CroppingShape::FIXED_ELLIPSE:
    drawEllipseOpacity();
    break;
  case CroppingShape::SQUARE:
  case CroppingShape::RECT:
    drawRectOpacity();
    drawSelectionFrame();
    break;
  case CroppingShape::CIRCLE:
  case CroppingShape::ELLIPSE:
    drawEllipseOpacity();
    drawSelectionFrame();
    break;
  }

  QPainter painter(this);
  painter.setPen(m_borderPen);
  painter.drawRect(m_croppedRect);
}

void ImageCropperQLabel::mousePressEvent(QMouseEvent *ev) {
  isLbuttonPressed = true;
  m_currMovedPos = m_pressedPos = ev->pos();
}

void ImageCropperQLabel::mouseMoveEvent(QMouseEvent *ev) {
  m_currMovedPos = ev->pos();

  CroppingPosition status = getPositionInCropperRect(m_currMovedPos);
  updateCursor(status);

  // no button pressed event
  // out of image rect boundary, no need to process!
  if (!isLbuttonPressed || !m_imageRect.contains(m_currMovedPos)) {
    return;
  }

  // update moved status!
  // isPositionUpdated = false;

  int x_offset = m_currMovedPos.x() - m_pressedPos.x();
  int y_offset = m_currMovedPos.y() - m_pressedPos.y();

  m_pressedPos = m_currMovedPos;

  // move cropper selection frame
  moveSelectionFrame(status, x_offset, y_offset);

  repaint();
}

void ImageCropperQLabel::mouseReleaseEvent(QMouseEvent *ev) {

  isLbuttonPressed = false;
  isPositionUpdated = true;
  setCursor(Qt::ArrowCursor);
}

// Opacity effect setting
// Draw other area as black(disabled / not selected)
void ImageCropperQLabel::drawOpacity(const QPainterPath &path) {

  QPainter painter(this);
  painter.setOpacity(m_opacity);
  painter.fillPath(path, QBrush(Qt::black));
}

void ImageCropperQLabel::drawSelectionFrame(bool isQuadSquare) {

  fillRectBlank(m_croppedRect.topLeft(), m_selectionSquareLength);
  fillRectBlank(m_croppedRect.topRight(), m_selectionSquareLength);
  fillRectBlank(m_croppedRect.bottomLeft(), m_selectionSquareLength);
  fillRectBlank(m_croppedRect.bottomRight(), m_selectionSquareLength);

  // design for Ellipse(ignore about this part now!)
  if (!isQuadSquare) {
  }
}

void ImageCropperQLabel::fillRectBlank(QPoint central, std::size_t length,
                                       QColor color) {
  QRect rect(central.x() - length * 0.5f, central.y() - length * 0.5f, length,
             length);
  QPainter painter(this);
  painter.fillRect(rect, color);
}

const bool
ImageCropperQLabel::isMouseInsideSelectionSquare(const QPoint &p1,
                                                 const QPoint &p2) const {

  return std::abs(p1.x() - p2.x()) * 2 <= m_selectionSquareLength &&
         std::abs(p1.y() - p2.y()) * 2 <= m_selectionSquareLength;
}

const CroppingPosition
ImageCropperQLabel::getPositionInCropperRect(const QPoint &p) {

  if (isMouseInsideSelectionSquare(
          p, QPoint(m_croppedRect.right(), m_croppedRect.center().y()))) {
    return CroppingPosition::RIGHT_MID;
  } else if (isMouseInsideSelectionSquare(
                 p, QPoint(m_croppedRect.left(), m_croppedRect.center().y()))) {
    return CroppingPosition::LEFT_MID;
  } else if (isMouseInsideSelectionSquare(p, QPoint(m_croppedRect.center().x(),
                                                    m_croppedRect.bottom()))) {
    return CroppingPosition::BUTTOM_MID;
  } else if (isMouseInsideSelectionSquare(
                 p, QPoint(m_croppedRect.center().x(), m_croppedRect.top()))) {
    return CroppingPosition::TOP_MID;
  } else if (isMouseInsideSelectionSquare(p, m_croppedRect.bottomLeft())) {
    return CroppingPosition::LEFT_BUTTOM;
  } else if (isMouseInsideSelectionSquare(p, m_croppedRect.bottomRight())) {
    return CroppingPosition::RIGHT_BUTTOM;
  } else if (isMouseInsideSelectionSquare(p, m_croppedRect.topLeft())) {
    return CroppingPosition::LEFT_TOP;
  } else if (isMouseInsideSelectionSquare(p, m_croppedRect.topRight())) {
    return CroppingPosition::RIGHT_TOP;
  } else if (m_croppedRect.contains(p, true)) {
    return CroppingPosition::INSIDE_RECT;
  }

  return CroppingPosition::OUTSIDE_RECT;
}

void ImageCropperQLabel::moveSelectionFrame(const CroppingPosition &pos,
                                            int x_offset, int y_offset) {

  if (pos == CroppingPosition::OUTSIDE_RECT)
    return;

  else if (pos == CroppingPosition::RIGHT_BUTTOM) {
    auto dx = m_currMovedPos.x() - m_croppedRect.left();
    auto dy = m_currMovedPos.y() - m_croppedRect.top();
    setCursor(Qt::SizeFDiagCursor);

    // there is not much changes in visual perspective, so return
    if (dx < m_cropperMinimumWidth && dy < m_cropperMinimumHeight) {
      return;
    }

    switch (m_shape) {
    case CroppingShape::RECT:
    case CroppingShape::ELLIPSE:
      if (dx >= m_cropperMinimumWidth) {
        m_croppedRect.setRight(m_currMovedPos.x());
      }
      if (dy >= m_cropperMinimumHeight) {
        m_croppedRect.setBottom(m_currMovedPos.y());
      }
      break;

    case CroppingShape::SQUARE:
    case CroppingShape::CIRCLE:
      if (dx > dy && dx + m_croppedRect.top() <= m_imageRect.bottom()) {
        m_croppedRect.setBottom(m_croppedRect.top() + dx);
        m_croppedRect.setRight(m_currMovedPos.x());
      } else if (dx <= dy && dy + m_croppedRect.left() <= m_imageRect.right()) {
        m_croppedRect.setBottom(m_currMovedPos.y());
        m_croppedRect.setRight(dy + m_croppedRect.left());
      }
      break;

    default:
      break;
    }
  } else if (pos == CroppingPosition::LEFT_BUTTOM) {
    auto dx = m_croppedRect.right() - m_currMovedPos.x();
    auto dy = m_currMovedPos.y() - m_croppedRect.top();
    setCursor(Qt::SizeBDiagCursor);

    // there is not much changes in visual perspective, so return
    if (dx < m_cropperMinimumWidth && dy < m_cropperMinimumHeight) {
      return;
    }

    switch (m_shape) {
    case CroppingShape::RECT:
    case CroppingShape::ELLIPSE:
      if (dx >= m_cropperMinimumWidth) {
        m_croppedRect.setLeft(m_currMovedPos.x());
      }
      if (dy >= m_cropperMinimumHeight) {
        m_croppedRect.setBottom(m_currMovedPos.y());
      }
      break;

    case CroppingShape::SQUARE:
    case CroppingShape::CIRCLE:
      if (dx > dy && dx + m_croppedRect.top() <= m_imageRect.bottom()) {
        m_croppedRect.setBottom(m_croppedRect.top() + dx);
        m_croppedRect.setLeft(m_currMovedPos.x());
      } else if (dx <= dy && m_croppedRect.right() - dy >= m_imageRect.left()) {
        m_croppedRect.setBottom(m_currMovedPos.y());
        m_croppedRect.setLeft(m_croppedRect.right() - dy);
      }
      break;

    default:
      break;
    }
  } else if (pos == CroppingPosition::LEFT_TOP) {
    auto dx = m_croppedRect.right() - m_currMovedPos.x();
    auto dy = m_croppedRect.bottom() - m_currMovedPos.y();
    setCursor(Qt::SizeFDiagCursor);

    // there is not much changes in visual perspective, so return
    if (dx < m_cropperMinimumWidth && dy < m_cropperMinimumHeight) {
      return;
    }

    switch (m_shape) {
    case CroppingShape::RECT:
    case CroppingShape::ELLIPSE:
      if (dx >= m_cropperMinimumWidth) {
        m_croppedRect.setLeft(m_currMovedPos.x());
      }
      if (dy >= m_cropperMinimumHeight) {
        m_croppedRect.setTop(m_currMovedPos.y());
      }
      break;
    case CroppingShape::SQUARE:
    case CroppingShape::CIRCLE:
      if (dx > dy && m_croppedRect.bottom() - dx <= m_imageRect.top()) {
        m_croppedRect.setTop(m_croppedRect.bottom() - dx);
        m_croppedRect.setLeft(m_currMovedPos.x());
      } else if (dx <= dy && m_croppedRect.right() - dy >= m_imageRect.left()) {
        m_croppedRect.setTop(m_currMovedPos.y());
        m_croppedRect.setLeft(m_croppedRect.right() - dy);
      }
      break;
    default:
      break;
    }
  } else if (pos == CroppingPosition::RIGHT_TOP) {
    auto dx = m_currMovedPos.x() - m_croppedRect.right();
    auto dy = m_croppedRect.bottom() - m_currMovedPos.y();
    setCursor(Qt::SizeBDiagCursor);

    // there is not much changes in visual perspective, so return
    if (dx < m_cropperMinimumWidth && dy < m_cropperMinimumHeight) {
      return;
    }

    switch (m_shape) {
    case CroppingShape::RECT:
    case CroppingShape::ELLIPSE:
      if (dx >= m_cropperMinimumWidth) {
        m_croppedRect.setRight(m_currMovedPos.x());
      }
      if (dy >= m_cropperMinimumHeight) {
        m_croppedRect.setTop(m_currMovedPos.y());
      }
      break;

    case CroppingShape::SQUARE:
    case CroppingShape::CIRCLE:
      if (dx < dy && dy + m_croppedRect.left() <= m_imageRect.right()) {
        m_croppedRect.setRight(dy + m_croppedRect.left());
        m_croppedRect.setTop(m_currMovedPos.y());
      } else if (dx >= dy && m_croppedRect.bottom() - dx >= m_imageRect.top()) {
        m_croppedRect.setBottom(m_currMovedPos.x());
        m_croppedRect.setLeft(m_croppedRect.bottom() - dx);
      }

      break;
    default:
      break;
    }
  } else if (pos == CroppingPosition::INSIDE_RECT) {

    if (x_offset > 0) {
      if (m_croppedRect.right() + x_offset > m_imageRect.right()) {
        x_offset = 0;
      }
    } else {
      if (m_croppedRect.left() + x_offset < m_imageRect.left()) {
        x_offset = 0;
      }
    }
    if (y_offset > 0) {
      if (m_croppedRect.bottom() + y_offset > m_imageRect.bottom()) {
        y_offset = 0;
      }
    } else {
      if (m_croppedRect.top() + y_offset < m_imageRect.top()) {
        y_offset = 0;
      }
    }

    m_croppedRect.moveTo(m_croppedRect.left() + x_offset,
                         m_croppedRect.top() + y_offset);
  }
}

void ImageCropperQLabel::drawRectOpacity() {

  QPainterPath image, cropped;
  image.addRect(m_imageRect);
  cropped.addRect(m_croppedRect);

  // exclude the other area except cropped area!
  // so, background is going to be darked!!
  //  background = image - cropped
  drawOpacity(image.subtracted(cropped));
}

void ImageCropperQLabel::drawEllipseOpacity() {

  QPainterPath image, cropped;
  image.addRect(m_imageRect);

  // if cropped' width = height, then its gonna to be a sphere!
  cropped.addEllipse(m_croppedRect);

  // exclude the other area except cropped area!
  // so, background is going to be darked!!
  //  background = image - cropped
  drawOpacity(image.subtracted(cropped));
}

void ImageCropperQLabel::resetCropper() {

  switch (m_shape) {
  case CroppingShape::UNDEFINED:
    break;
  case CroppingShape::FIXED_RECT:
  case CroppingShape::FIXED_ELLIPSE:
    break;
  case CroppingShape::SQUARE:
  case CroppingShape::RECT:
  case CroppingShape::CIRCLE:
  case CroppingShape::ELLIPSE:
    auto width = m_calibratedImage.width();
    auto height = m_calibratedImage.height();

    auto label_width = this->width();
    auto label_height = this->height();

    // find the smallest among width and height
    std::size_t length =
        static_cast<std::size_t>(0.75f * (width > height ? height : width));
    m_croppedRect.setRect(static_cast<int>(label_width * 0.5 - length * 0.5f),
                          static_cast<int>(label_height * 0.5f - length * 0.5f),
                          length, length);
    break;
  }
}
