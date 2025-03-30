#ifndef LOGICEXECUTOR_H
#define LOGICEXECUTOR_H

#include <QObject>
#include <def.hpp>
#include <QJsonObject>
#include <unordered_map>

class LogicExecutor : public QObject
{
    Q_OBJECT

    using Callbackfunction = std::function<void(QJsonObject &&)>;

public:
    LogicExecutor();
    virtual ~LogicExecutor();

private:
    void registerCallbacks();

signals:

private:
    /*according to service type to execute callback*/
    std::map<ServiceType, Callbackfunction> m_callbacks;
};

#endif // LOGICEXECUTOR_H
