#ifndef LOGICMETHOD_H
#define LOGICMETHOD_H

#include <QObject>
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

signals:

private:

};

#endif // LOGICMETHOD_H
