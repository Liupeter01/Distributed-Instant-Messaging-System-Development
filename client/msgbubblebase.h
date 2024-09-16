#ifndef MSGBUBBLEBASE_H
#define MSGBUBBLEBASE_H

#include <QFrame>
#include <QWidget>
#include <QHBoxLayout>
#include <QPaintEvent>

enum class ChattingRole{
    Sender, /*message sender*/
    Receiver/*message receiver*/
};

/*bubble base class*/
class MsgBubbleBase : public QFrame
{
    Q_OBJECT

public:
    explicit MsgBubbleBase(ChattingRole role, QWidget *parent = nullptr);
    ~MsgBubbleBase();

public:
    /*setup margin for both top and left(right)*/
    static constexpr std::size_t margin = 3;

    /*message bubble triangle*/
    static constexpr std::size_t triangle_width = 8;

    /*load widget for bubble
     * (it could be QLabel & QFrame)
    */
    void setupBubbleWidget(QWidget *widget);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ChattingRole m_role;
    QHBoxLayout *m_layout;
};

#endif // MSGBUBBLEBASE_H