#ifndef FILETCPNETWORK_H
#define FILETCPNETWORK_H

#include "tcpnetworkbase.h"
#include <QObject>
#include <resourcestoragemanager.h>

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

public:
  void send_download_request(std::shared_ptr<FileTransferDesc> info);

protected:
  void registerNetworkEvent() override;
  void registerCallback() override;
  void registerMetaType() override;

signals:
  /*
   * signal_breakpoint_upload/download could serve for two main purposes
   * - indicate the process of file upload response, and start to prepare for
   * the next block!
   * - when user activate break point resume, it will start from the curr_size
   * of the file
   */

  void signal_breakpoint_upload(std::shared_ptr<FileTransferDesc> desc);

  /*
   * slot_breakpoint_download:
   * - The user should use it to write block_data to specific position in the
   * file
   * - this function will also update the downloading status in the
   * unordered_map
   */
  void
  signal_breakpoint_download(std::shared_ptr<FileTransferDesc> updated_desc,
                             QByteArray decoded_data,
                             const std::size_t block_size);

private slots:
  void slot_terminate_server() override;
  void slot_connect2_server() override;
};

#endif // FILETCPNETWORK_H
