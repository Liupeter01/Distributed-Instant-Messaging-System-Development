#pragma once
#ifndef LOGICMETHOD_H
#define LOGICMETHOD_H
#include <QObject>
#include <QThread>
#include <atomic>
#include <logicexecutor.h> /*execute some specfic logic from network*/
#include <resourcestoragemanager.h>

class LogicMethod : public QObject, public Singleton<LogicMethod> {

  Q_OBJECT
  friend class Singleton<LogicMethod>;

public:
  virtual ~LogicMethod() { m_thread->quit(); }

public:
  bool getPauseStatus() const;
  void setPause(const bool status);

private:
  explicit LogicMethod(QObject *parent = nullptr);
  void registerSignals();
  void registerMetaType();

signals:
  // start transmission(with init filename & filepath)
  void signal_start_file_upload(const QString &fileName,
                                      const QString &filePath,
                                      const std::size_t fileChunk = 4096);

  // pause transmission
  void signal_pause_file_upload();

  // resume transmission
  void signal_resume_file_upload(const QString &fileName,const QString &filePath);

  //update all UI interfaces that relevant to avatar icons(qlabels)
  void signal_update_interfaces_avatar_icons(const QString& path);

private slots:

private:
  QThread *m_thread;
  LogicExecutor *m_exec;

  // transmission stopped
  std::atomic<bool> m_pause = false;
};

#endif // LOGICMETHOD_H
