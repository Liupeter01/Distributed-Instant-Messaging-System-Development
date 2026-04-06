#pragma once
#ifndef LOGICMETHOD_H
#define LOGICMETHOD_H

#include <QFileInfo>
#include <QObject>
#include <QThread>
#include <atomic>
#include <logicexecutor.h> /*execute some specfic logic from network*/
#include <optional>
#include <singleton.hpp>
#include <unordered_map>

class LogicMethod : public QObject, public Singleton<LogicMethod> {

  Q_OBJECT
  friend class Singleton<LogicMethod>;

public:
  virtual ~LogicMethod() { m_thread->quit(); }

public:
  bool getPauseStatus() const;
  void setPause(const bool status);
  void recordMD5Progress(const QString &md5, std::shared_ptr<QFileInfo> info);

  [[nodiscard]]
  std::optional<std::shared_ptr<QFileInfo>> getFileByMD5(const QString &md5);

private:
  explicit LogicMethod(QObject *parent = nullptr);
  void registerSignals();
  void registerMetaType();

signals:
  // start transmission(with init filename & filepath)
  void signal_start_file_transmission(const QString &fileName,
                                      const QString &filePath,
                                      const std::size_t fileChunk = 4096);

  // pause transmission
  void signal_pause_file_transmission();

  // resume transmission
  void signal_resume_file_transmission();

  /*
   * signal_break_point_resume could serve for two main purposes
   * - indicate the process of file upload response, and start to prepare for the next block!
   * - when user activate break point resume, it will start from the curr_size of the file
   */
  void signal_break_point_resume(QString checksum,
                                       const std::size_t curr_seq,
                                       const std::size_t curr_size,
                                       const std::size_t total_size,
                                       const bool eof);

private slots:
  void slot_resources_logic_handler(const uint16_t id, QJsonObject obj);

private:
  QThread *m_thread;
  LogicExecutor *m_exec;

  // transmission stopped
  std::atomic<bool> m_pause = false;

  // mutex
  std::mutex m_mtx;

  // store info in mapping structure
  std::unordered_map<
      /*    md5 = */ QString,
      /*QFileInfo=*/std::shared_ptr<QFileInfo>>
      m_md5_cache;
};

#endif // LOGICMETHOD_H
