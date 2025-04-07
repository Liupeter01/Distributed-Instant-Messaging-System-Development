#ifndef LOGICEXECUTOR_H
#define LOGICEXECUTOR_H

#include <QJsonObject>
#include <QObject>
#include <def.hpp>
#include <unordered_map>

class LogicMethod;

class LogicExecutor : public QObject {
  Q_OBJECT
  friend class LogicMethod;
  using Callbackfunction = std::function<void(const QJsonObject)>;

public:
  LogicExecutor();
  virtual ~LogicExecutor();

private:
  void registerCallbacks();

signals:
  /*data transmission status*/
  void signal_data_transmission_status(const QString &filename,
                                       const std::size_t curr_seq,
                                       const std::size_t curr_size,
                                       const std::size_t total_size,
                                        const bool eof);

private slots:
  /*forward resources server's message to a standlone logic thread*/
  void slot_resources_logic_handler(const uint16_t id, const QJsonObject obj);

private:
  /*according to service type to execute callback*/
  std::map<ServiceType, Callbackfunction> m_callbacks;
};

#endif // LOGICEXECUTOR_H
