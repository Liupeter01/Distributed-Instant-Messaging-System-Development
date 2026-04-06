#ifndef FILETCPNETWORK_H
#define FILETCPNETWORK_H

#include "tcpnetworkbase.h"
#include <QObject>

class FileTCPNetwork : public TCPNetworkBase,
                       public Singleton<FileTCPNetwork>,
                       public std::enable_shared_from_this<FileTCPNetwork>

{
  Q_OBJECT
  friend class Singleton<FileTCPNetwork>;

public:
  virtual ~FileTCPNetwork() = default;

private:
  explicit FileTCPNetwork();

protected:
  void registerNetworkEvent() override;
  void registerCallback() override;
  void registerMetaType() override;
  void readyReadHandler(const uint16_t id, QJsonObject &&obj) override;

signals:
  /* forward resources server's message to a standlone logic thread */
    void signal_resources_logic_handler(const uint16_t id, QJsonObject obj);

private slots:
  void slot_terminate_server() override;
  void slot_connect2_server() override;
};

#endif // FILETCPNETWORK_H
