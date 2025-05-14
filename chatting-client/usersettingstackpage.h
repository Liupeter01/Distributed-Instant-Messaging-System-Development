#ifndef USERSETTINGSTACKPAGE_H
#define USERSETTINGSTACKPAGE_H

#include <QWidget>

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
    void on_upload_avator_clicked();

private:
    Ui::UserSettingStackPage *ui;
};

#endif // USERSETTINGSTACKPAGE_H
