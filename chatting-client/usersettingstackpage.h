#ifndef USERSETTINGSTACKPAGE_H
#define USERSETTINGSTACKPAGE_H

#include <QPixmap>
#include <QWidget>

namespace Ui {
class UserSettingStackPage;
}

class UserSettingStackPage : public QWidget {
  Q_OBJECT

public:
  explicit UserSettingStackPage(QWidget *parent = nullptr);
  ~UserSettingStackPage();

signals:
  void signal_start_file_transmission(const QString &fileName,
                                      const QString &filePath,
                                      const std::size_t fileChunk = 4096);

private:
  void registerSignal();

private slots:
  void on_submit_clicked();
  void on_select_avator_clicked();

private:
  Ui::UserSettingStackPage *ui;
  QPixmap m_avator;

  QString m_filePath;
  QString m_fileName;
};

#endif // USERSETTINGSTACKPAGE_H
