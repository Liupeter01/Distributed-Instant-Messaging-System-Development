#include "logicexecutor.h"
#include <QDebug>
#include <QJsonDocument>

LogicExecutor::LogicExecutor()
{
    registerCallbacks();
}

LogicExecutor::~LogicExecutor()
{}

void LogicExecutor::registerCallbacks(){
    m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
        ServiceType::SERVICE_FILEUPLOADRESPONSE, [this](QJsonObject &&json) {
            /*error occured!*/
            if (!json.contains("error")) {
                qDebug() << "Json Parse Error!";
                return;
            }
            if (json["error"].toInt() !=
                static_cast<int>(ServiceStatus::SERVICE_SUCCESS)) {
                qDebug() << "Login Server Error!";
                return;
            }
    }));

}

