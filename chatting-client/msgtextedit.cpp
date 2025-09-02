#include "msgtextedit.h"
#include <QDebug>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QTextCursor>
#include <ChattingThreadDef.hpp>

MsgTextEdit::MsgTextEdit(QWidget *parent) : QTextEdit(parent) {}

MsgTextEdit::~MsgTextEdit() {}

const QVector<MsgInfo> &MsgTextEdit::getMsgList() {
  /*clear processed storage*/
  m_processed_list.clear();

  std::size_t url_index{0};
  std::size_t ammount = m_temporary_list.size();

  QString text = "";
  QString plaintext = this->document()->toPlainText();

  /* iterates over each character in the plain text of the document */
  for (std::size_t index = 0; index < plaintext.size(); ++index) {

    /*
     * Detect if the character represents an embedded object (likely an image)
     * we need to know whether its likely to be an image or not
     */
    if (plaintext[index] == QChar::ObjectReplacementCharacter) {

      // If there's accumulated text, append it to the list as a "text" message
      if (!text.isEmpty()) {
        MsgInfo info = {MsgType::TEXT, text, QPixmap()};

        insertMsgList(std::move(info));
        text.clear();
      }
      // Now process the image
      while (url_index < ammount) {
        MsgInfo msg = m_temporary_list[url_index];
        url_index++;

        // Check if the image path (msg.content) is present in the HTML
        if (this->document()->toHtml().contains(msg.content,
                                                Qt::CaseSensitive)) {
          m_processed_list.append(msg);
          break;
        }
      }
    } else {
      // Accumulate plain text
      text.append(plaintext[index]);
    }
  }

  /*handle remainning*/
  if (!text.isEmpty()) {
    MsgInfo info = {MsgType::TEXT, text, QPixmap()};

    insertMsgList(std::move(info));
    text.clear();
  }

  m_temporary_list.clear();
  this->clear();
  return m_processed_list;
}

QStringList MsgTextEdit::getURL(QString text) {
  QStringList urls;
  if (text.isEmpty())
    return urls;

  QStringList list = text.split("\n");
  foreach (QString url, list) {
    if (!url.isEmpty()) {
      QStringList str = url.split("///");
      if (str.size() >= 2)
        urls.append(str.at(1));
    }
  }
  return urls;
}

QString MsgTextEdit::getFileSize(qint64 size) {
  QString Unit;
  double num;
  if (size < 1024) {
    num = size;
    Unit = "B";
  } else if (size < 1024 * 1224) {
    num = size / 1024.0;
    Unit = "KB";
  } else if (size < 1024 * 1024 * 1024) {
    num = size / 1024.0 / 1024.0;
    Unit = "MB";
  } else {
    num = size / 1024.0 / 1024.0 / 1024.0;
    Unit = "GB";
  }
  return QString::number(num, 'f', 2) + " " + Unit;
}

QPixmap MsgTextEdit::getFileIconPixmap(const QString &path) {
  QFileIconProvider provder;
  QFileInfo fileinfo(path);

  /*get file icon*/
  QIcon icon = provder.icon(fileinfo);

  /*get file size info*/
  QString strFileSize = getFileSize(fileinfo.size());
  qDebug() << "FileSize=" << fileinfo.size();

  QFont font(QString("Times New Roman"), 10, QFont::Normal, false);
  QFontMetrics fontMetrics(font);
  QSize textSize = fontMetrics.size(Qt::TextSingleLine, fileinfo.fileName());

  QSize FileSize = fontMetrics.size(Qt::TextSingleLine, strFileSize);
  int maxWidth =
      textSize.width() > FileSize.width() ? textSize.width() : FileSize.width();
  QPixmap pix(50 + maxWidth + 10, 50);
  pix.fill();

  QPainter painter;
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(font);
  painter.begin(&pix);

  /*file icon*/
  QRect rect(0, 0, 50, 50);
  painter.drawPixmap(rect, icon.pixmap(40, 40));
  painter.setPen(Qt::black);

  /*file name*/
  QRect rectText(50 + 10, 3, textSize.width(), textSize.height());
  painter.drawText(rectText, fileinfo.fileName());

  /*file size*/
  QRect rectFile(50 + 10, textSize.height() + 5, FileSize.width(),
                 FileSize.height());
  painter.drawText(rectFile, strFileSize);
  painter.end();
  return pix;
}

void MsgTextEdit::keyPressEvent(QKeyEvent *e) {
  /*when user press enter and return*/
  if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
    /*emit sending signal*/
    emit send();
    return;
  }
  QTextEdit::keyPressEvent(e);
}

void MsgTextEdit::dragEnterEvent(QDragEnterEvent *e) {
  if (e->source() == this)
    e->ignore();
  else
    e->accept();

  QTextEdit::dragEnterEvent(e);
}

void MsgTextEdit::dropEvent(QDropEvent *e) {
  insertFromMimeData(e->mimeData());
  e->accept();

  /*we do not need to use base class's dropevent*/
}

bool MsgTextEdit::canInsertFromMimeData(const QMimeData *source) const {
  return QTextEdit::canInsertFromMimeData(source);
}

void MsgTextEdit::insertFromMimeData(const QMimeData *source) {
  QStringList list = getURL(source->text());

  if (list.isEmpty()) {
    return;
  }

  foreach (QString url, list) {
    if (isImage(url))
      insertImage(url);
    else
      insertTextFile(url);
  }
}

bool MsgTextEdit::isImage(const QString &path) {
  QStringList formatList = imageFormat.split(',');

  /* check file info, check file suffix */
  QFileInfo file(path);

  /*file is not even exist*/
  if (!file.exists()) {
    qDebug() << "file is not even exist!";
    return false;
  }

  /*check suffix contains or not*/
  return formatList.contains(file.suffix(), Qt::CaseInsensitive);
}

void MsgTextEdit::insertImage(const QString &url) {
  QImage image(url);

  /*scale ratio*/
  if (image.width() > PICTURE_WIDTH || image.height() > PICTURE_HEIGHT) {
    if (image.width() > image.height())
      image = image.scaledToWidth(PICTURE_WIDTH, Qt::SmoothTransformation);
    else
      image = image.scaledToHeight(PICTURE_HEIGHT, Qt::SmoothTransformation);
  }

  QTextCursor cursor = this->textCursor();
  cursor.insertImage(image, url);

  MsgInfo info = {MsgType::IMAGE, url, QPixmap::fromImage(image)};

  m_temporary_list.append(std::move(info));
}

void MsgTextEdit::insertTextFile(const QString &path) {
  /*load file*/
  QFileInfo file(path);

  /*file is not even exist*/
  if (!file.exists()) {
    QMessageBox::information(this, "Error", "File is not exist!");
    qDebug() << "file is not even exist!";
    return;
  }

  if (!file.isDir()) {
    QMessageBox::information(this, "Error", "you are selecting a directory!");
    qDebug() << "you are selecting a directory!";
    return;
  }

  /*file size over 100MB*/
  if (file.size() > 100 * 1024 * 1024) {
    QMessageBox::information(this, "Error", "file size over 100M!");
    qDebug() << "file size over 100M!";
    return;
  }

  QPixmap picture = getFileIconPixmap(path);
  QTextCursor cursor = this->textCursor();
  cursor.insertImage(picture.toImage(), path);

  MsgInfo info = {MsgType::FILE, path, picture};

  m_temporary_list.append(std::move(info));
}

void MsgTextEdit::insertMultiItems(const QStringList &lists) {
  if (lists.empty()) {
    return;
  }
  foreach (QString url, lists) {
    if (isImage(url))
      insertImage(url);
    else
      insertTextFile(url);
  }
}

void MsgTextEdit::insertMsgList(MsgInfo &&info) {
  m_processed_list.append(std::move(info));
}
