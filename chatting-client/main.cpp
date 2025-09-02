#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QString>
#include "tcpnetworkthread.h"

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  /*Theme css file*/
  QFile qss(QT_DEMO_HOME "/style/theme.qss");
  if (qss.open(QFile::ReadOnly)) {
    qDebug() << "Open Success!\n";
    a.setStyleSheet(QString(qss.readAll()));
    qss.close();
  } else {
    qDebug() << "Open Failed!\n";
  }

  MainWindow w;

  //seperated network thread
  TCPNetworkThread network_th;
  w.show();
  return a.exec();
}
