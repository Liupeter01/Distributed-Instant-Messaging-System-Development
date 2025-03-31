#include "logicexecutor.h"
#include <QDebug>
#include <QJsonDocument>

LogicExecutor::LogicExecutor() { registerCallbacks(); }

LogicExecutor::~LogicExecutor() {}

void LogicExecutor::registerCallbacks() {

  m_callbacks.insert(std::pair<ServiceType, Callbackfunction>(
      ServiceType::SERVICE_FILEUPLOADRESPONSE, [this](const QJsonObject json) {
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

        [[maybe_unused]] auto filename = json["filename"].toString();
        [[maybe_unused]] auto curr_seq = json["curr_seq"].toInt();
        [[maybe_unused]] auto curr_size = json["curr_size"].toInt();
        [[maybe_unused]] auto total_size = json["total_size"].toInt();

        /*notifying the main UI interface to update progress bar!*/
        emit signal_data_transmission_status(filename, curr_seq, curr_size,
                                             total_size);
      }));
}

void LogicExecutor::slot_resources_logic_handler(const uint16_t id,
                                                 const QJsonObject obj) {

  try {
    m_callbacks[static_cast<ServiceType>(id)](obj);

  } catch (const std::exception &e) {
    qDebug() << e.what();
  }
}
