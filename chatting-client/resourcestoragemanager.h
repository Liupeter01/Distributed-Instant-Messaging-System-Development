#ifndef RESOURCESTORAGEMANAGER_H
#define RESOURCESTORAGEMANAGER_H
#include "singleton.hpp"
#include <QFileInfo>
#include <QLabel>
#include <QString>
#include <UserDef.hpp>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

enum class TransferDirection { Download, Upload };

struct FileTransferDesc {
  FileTransferDesc(const QString &_filename, const QString &_checksum,
                   const QString &_filePath, const std::size_t _curr_sequence,
                   const std::size_t _last_sequence, const bool _eof,
                   const std::size_t _transfered_size,
                   const std::size_t _total_size,
                   TransferDirection _direction = TransferDirection::Upload)

      : filename(_filename), checksum(_checksum), curr_sequence(_curr_sequence),
        last_sequence(_last_sequence), transfered_size(_transfered_size),
        filePath(_filePath), total_size(_total_size), isEOF(_eof),
        key(_filename + QString("_") + _checksum), direction(_direction) {}

  QString key; //=filename_checksum

  QString filename;
  QString filePath;
  QString checksum;

  std::size_t curr_sequence;
  std::size_t last_sequence;
  bool isEOF;

  std::size_t transfered_size;
  std::size_t total_size;

  TransferDirection direction = TransferDirection::Upload;
};

struct FileTransferControlBlock : public FileTransferDesc {

  FileTransferControlBlock(const FileTransferDesc &o, const QString &block)
      : FileTransferDesc(o), block_data(block) {}

  QString block_data;
};

class ResourceStorageManager : public Singleton<ResourceStorageManager> {
  friend class Singleton<ResourceStorageManager>;

public:
  ~ResourceStorageManager();
  void setUserInfo(std::shared_ptr<UserNameCard> info);

  void set_host(const QString &_host) { m_info.host = _host; }
  void set_port(const QString &_port) { m_info.port = _port; }
  // void set_token(const QString &_token) { m_info.token = _token; }
  void set_uuid(const QString &_uuid) { m_info.uuid = _uuid; }

  const QString &get_host() const { return m_info.host; }
  const QString &get_port() const { return m_info.port; }
  // const QString &get_token() const { return m_info.token; }
  const QString get_uuid() const { return m_info.uuid; }

public:
  void recordUnfinishedTask(const QString &checksum,
                            std::shared_ptr<FileTransferDesc> info);

  [[nodiscard]]
  std::optional<std::shared_ptr<FileTransferDesc>>
  getUnfinishedTasks(const QString &str);

  bool removeUnfinishedTask(const QString &str);
  bool isDownloading(const QString &str);

  bool recordQLabelUpdateLists(const QString &path, QLabel *label);
  bool executeQLabelUpdateLists(const QString &path);
  void removeQLabelUpdateLists(const QString &path);

private:
  ResourceStorageManager();

private:
  struct ResourcesServerInfo {
    ResourcesServerInfo() : uuid(), host(), port() {}

    QString uuid;
    QString host;
    QString port;
  } m_info;

  /*store current user's info*/
  std::shared_ptr<UserNameCard> m_userInfo;

  // mutex
  std::mutex m_mtx;

  // store info in mapping structure
  std::unordered_map<
      /*
       * checksum or filename
       * - upload: checksum
       * - download: filename
       */
      QString,

      /*FileTransferDesc=*/
      std::shared_ptr<FileTransferDesc>>

      m_unfinished_tasks;

  /* hashmap + linklist
   *   store the relationship between filename(useravatar) and qlabel objects!!!
   */
  std::unordered_map<
      /*filename*/
      QString,

      /*vector of qlabels*/
      std::vector<std::shared_ptr<QLabel>>>

      m_batch_qlabels;
};

#endif // RESOURCESTORAGEMANAGER_H
