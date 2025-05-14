#ifndef IMAGECROPPERQLABEL_H
#define IMAGECROPPERQLABEL_H

#include <QLabel>
#include <QWidget>
#include <QObject>
#include <QPixmap>
#include <QSize>
#include <QRect>
#include <optional>
#include <QPen>

enum class CroppingShape{
    UNDEFINED,
    FIXED_RECT,
    FIXED_ELLIPSE,
    RECT,
    SQUARE,
    ELLIPSE,
    CIRCLE
};

enum class CroppingPosition{
    OUTSIDE_RECT,
    INSIDE_RECT,
    LEFT_TOP,
    LEFT_MID,
    LEFT_BUTTOM,
    RIGHT_TOP,
    RIGHT_MID,
    RIGHT_BUTTOM,
    BUTTOM_MID,
    TOP_MID
};

class ImageCropperQLabel : public QLabel
{
    Q_OBJECT
public:
    ImageCropperQLabel(QWidget* parent = nullptr);
    virtual ~ImageCropperQLabel();

public:
    void setCropperSize(const QSize &size);
    void setCropperSize(const std::size_t _width, const std::size_t _height);
    void setCropperShape(const CroppingShape shape);

    void setOriginalPixmap(const QPixmap& image);

    std::optional<QPixmap> getCropppedImage();
    void updateCursor(const CroppingPosition& pos);

protected:
    void resetCropper();
    std::optional<QPixmap> getCropppedImage(const CroppingShape shape);
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void drawRectOpacity();
    void drawEllipseOpacity();

private:
    void drawOpacity(const QPainterPath & path);
    void drawSelectionFrame(bool isQuadSquare = true);
    void fillRectBlank(QPoint central, std::size_t length, QColor color = Qt::white);
    [[nodiscard]]const bool isMouseInsideSelectionSquare(const QPoint& p1, const QPoint& p2) const;
    [[nodiscard]] const CroppingPosition getPositionInCropperRect(const QPoint& p);
    void moveSelectionFrame(const CroppingPosition& pos, int x_offset, int y_offset);

private:
    QPen m_borderPen;

    bool isPositionUpdated = true; //did the mouse just move to other locations?
    bool isLbuttonPressed = false;
    QPoint m_pressedPos;        //the last pressed pos in mousepressevent
    QPoint m_currMovedPos;      //the last pressed pos in mousepressevent

    CroppingShape m_shape;
    const qreal m_opacity = 0.4f;

    //the length of square on the corner of selection square
    const std::size_t m_selectionSquareLength = 8;
    const std::size_t m_cropperMinimumWidth = m_selectionSquareLength * 2;
    const std::size_t m_cropperMinimumHeight = m_selectionSquareLength * 2;

    float m_ratio;
    QSize m_size;               //qlabel size
    QRect m_imageRect;
    QRect m_croppedRect;
    QPixmap m_originalImage;    //user pass the parameter
    QPixmap m_calibratedImage;  //after we cropped the original one
};

#endif // IMAGECROPPERQLABEL_H
