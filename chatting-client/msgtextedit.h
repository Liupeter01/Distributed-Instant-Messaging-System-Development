#pragma once
#ifndef MSGTEXTEDIT_H
#define MSGTEXTEDIT_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPixmap>
#include <QStringList>
#include <QTextEdit>
#include <QVector>
#include <QWidget>

enum class MsgType;

struct MsgInfo {
  MsgType type;
  QString content; // text msg or path to image
  QPixmap pixmap;  //
};

class MsgTextEdit : public QTextEdit {
  Q_OBJECT

public:
  explicit MsgTextEdit(QWidget *parent = nullptr);
  virtual ~MsgTextEdit();

signals:
  /*when pressing enter & return, then send the image*/
  void send();

public:
  const QVector<MsgInfo> &getMsgList();
  QStringList getURL(QString text);
  QString getFileSize(qint64 size);
  QPixmap getFileIconPixmap(const QString &path);

  static constexpr std::size_t PICTURE_WIDTH = 160;
  static constexpr std::size_t PICTURE_HEIGHT = 90;

  /*file size 100MB*/
  static constexpr std::size_t FILE_SIZE_MAX = 100 * 1024 * 1024;

  QString imageFormat = "bmp,jpg,png,tif,gif,pcx,tga,exif,fpx,svg,psd,cdr,pcd,"
                        "dxf,ufo,eps,ai,raw,wmf,webp";

protected:
  virtual void keyPressEvent(QKeyEvent *e) override;
  virtual void dragEnterEvent(QDragEnterEvent *e) override;
  virtual void dropEvent(QDropEvent *e) override;
  virtual bool canInsertFromMimeData(const QMimeData *source) const override;
  virtual void insertFromMimeData(const QMimeData *source) override;

private:
  /*check file info, check file suffix*/
  bool isImage(const QString &path);

  void insertImage(const QString &url);
  void insertTextFile(const QString &path);
  void insertMultiItems(const QStringList &lists);

  void insertMsgList(MsgInfo &&info);

private:
  QVector<MsgInfo> m_temporary_list;
  QVector<MsgInfo> m_processed_list;
};

#endif // MSGTEXTEDIT_H
