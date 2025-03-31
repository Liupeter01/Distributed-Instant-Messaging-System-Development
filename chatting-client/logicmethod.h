#ifndef LOGICMETHOD_H
#define LOGICMETHOD_H

#include <QObject>
#include <QThread>
#include <singleton.hpp>
/*execute some specfic logic from network*/
#include <logicexecutor.h>

class LogicMethod
    : public QObject
    , public Singleton<LogicMethod>
{
    Q_OBJECT
    friend class Singleton<LogicMethod>;

public:
    virtual ~LogicMethod();

private:
    explicit LogicMethod(QObject *parent = nullptr);
    void registerSignals();

signals:
    /*forward resources server's message to a standlone logic thread*/
    void signal_resources_logic_handler(const uint16_t id, const QJsonObject obj);

    /*data transmission status*/
    void signal_data_transmission_status(const QString& filename,
                                         const std::size_t curr_seq,
                                         const std::size_t curr_size,
                                         const std::size_t total_size);

private:
    QThread * m_thread;
    LogicExecutor* m_exec;
};

#endif // LOGICMETHOD_H
