#ifndef USERSETTINGSTACKPAGE_H
#define USERSETTINGSTACKPAGE_H

#include <QWidget>
#include <QPixmap>

namespace Ui {
class UserSettingStackPage;
}

class UserSettingStackPage : public QWidget
{
    Q_OBJECT

public:
    explicit UserSettingStackPage(QWidget *parent = nullptr);
    ~UserSettingStackPage();

private slots:
    void on_submit_clicked();
    void on_select_avator_clicked();

private:
    Ui::UserSettingStackPage *ui;
    QPixmap m_avator;
};

#endif // USERSETTINGSTACKPAGE_H
