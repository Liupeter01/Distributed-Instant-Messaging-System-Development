#ifndef TCPNETWORKCONNECTION_H
#define TCPNETWORKCONNECTION_H

#include "def.hpp"
#include <ChattingThreadDef.hpp>
#include <MsgNode.hpp>
#include <QJsonObject>
#include <QObject> //connect
#include <QString>
#include <QTcpSocket>
#include <QUrl>
#include <QtEndian>
#include <functional>
#include <optional>
#include <singleton.hpp>

struct UserNameCard;
struct UserFriendRequest;
struct ChatThreadPageResult;
enum class MsgType;

enum class TargetServer { CHATTINGSERVER, RESOURCESSERVER };

class TCPNetworkConnection
    : public QObject,
      public Singleton<TCPNetworkConnection>,
      public std::enable_shared_from_this<TCPNetworkConnection> {

  Q_OBJECT

  friend class Singleton<TCPNetworkConnection>;
  using Callbackfunction = std::function<void(QJsonObject &&)>;
  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;
  using RecvNodeType = RecvNode<QByteArray, ByteOrderConverter>;

  using ChattingBaseType = ChattingRecordBase;

  struct RecvInfo {
    uint16_t _id = 0;
    uint16_t _length = 0;
    QByteArray _msg;
  };

public:
  virtual ~TCPNetworkConnection();

  /*use signal to trigger data sending*/
  void send_data(SendNodeType &&data,
                 TargetServer tar = TargetServer::CHATTINGSERVER);

  /*
   * Send signals to a unified slot function for processing,
   * implementing a queue mechanism and ensuring thread safety.
   */
  void send_sequential_data_f(std::shared_ptr<SendNodeType> data,
                              TargetServer tar = TargetServer::CHATTINGSERVER) {
    emit signal_send_message(data, tar);
  }

  static void send_buffer(ServiceType type, QJsonObject &&obj);

private:
  TCPNetworkConnection();

  void registerNetworkEvent();
  void registerSocketSignal();
  void registerCallback();
  void registerErrorHandling();

protected:
  void setupChattingDataRetrieveEvent(QTcpSocket &socket, RecvInfo &received,
                                      RecvNodeType &buffer);

  void setupResourcesDataRetrieveEvent(QTcpSocket &socket, RecvInfo &received,
                                       RecvNodeType &buffer);

private slots:
  void slot_connect2_chatting_server();
  void slot_connect2_resources_server();

  void slot_terminate_chatting_server(const QString &uuid,
                                      const QString &token);
  void slot_terminate_resources_server();
  void terminate_chatting_server();

  /*Send signals to a unified slot function for processing,
   * implementing a queue mechanism and ensuring thread safety.
   */
  void slot_send_message(std::shared_ptr<SendNodeType> data,
                         TargetServer tar = TargetServer::CHATTINGSERVER);

  // void slot_bytes_written(qint64 bytes);

signals:
  void signal_connect2_chatting_server();
  void signal_connect2_resources_server();

  void signal_terminate_resources_server();

  /*send logout network message only!*/
  void signal_teminate_chatting_server(const QString &uuid,
                                       const QString &token);

  /*forward resources server's message to a standlone logic thread*/
  void signal_resources_logic_handler(const uint16_t id, const QJsonObject obj);

  /*Send signals to a unified slot function for processing,
   * implementing a queue mechanism and ensuring thread safety.
   */
  void signal_send_message(std::shared_ptr<SendNodeType> data,
                           TargetServer tar = TargetServer::CHATTINGSERVER);

  void signal_block_send(const std::size_t size);

  /*return connection status to login class*/
  void signal_connection_status(bool status);

  /*login to server failed*/
  void signal_login_failed(ServiceStatus status);

  /*logout from server*/
  void signal_logout_status(bool status);

  /*if login success, then switch to chatting dialog*/
  void signal_switch_chatting_dialog();

  /*
   * client sent search username request to server
   * server return result back to client
   */
  void signal_search_username(std::optional<std::shared_ptr<UserNameCard>>,
                              ServiceStatus status);

  /* client who is going to receive new friend request*/
  void signal_incoming_friend_request(
      std::optional<std::shared_ptr<UserFriendRequest>> info);

  /*client who inited request will receive response from here*/
  void signal_sender_response(bool status);

  /*client who is going to confirm request will receive status from here*/
  void signal_confirm_response(bool status);

  /*server be able to send friend request list to this client*/
  void signal_init_friend_request_list();

  /*server be able to send authenticate friend list to this client*/
  void signal_init_auth_friend_list();

  /*
   * emit a signal to add this person as a friend
   */
  void signal_add_auth_friend_to_contact_list(std::shared_ptr<UserNameCard>);

  /*
   * emit a signal to attach auth-friend messages to chatting history
   * This is the first offical chatting record,
   * so during this phase, "thread_id" will be dstributed to this chatting
   * thread!
   */
  void signal_add_auth_friend_init_chatting_thread(
      const UserChatType type, const QString &thread_id,
      std::shared_ptr<UserNameCard>,
      std::vector<std::shared_ptr<FriendingConfirmInfo>> list);

  /**
   * @brief signal_update_local2verification_status
   * emit a signal to ChattingDlgMainFrame class to update local msg status
   * which returns an allocated msg_id to replace local uuid
   * @param thread_id
   * @param uuid
   * @param msg_id
   */
  void signal_update_local2verification_status(const QString &thread_id,
                                               const QString &uuid,
                                               const QString &msg_id);

  /*
   * sender sends chat msg to receiver
   * sender could be a user who is not in the chathistorywidget list
   * so we have to create a new widget for him
   */
  void signal_incoming_msg(MsgType type, std::shared_ptr<ChattingBaseType> msg);

  /*
   * This function is mainly for the main interface
   * to update it's Chat Thread
   */
  void signal_update_chat_thread(std::shared_ptr<ChatThreadPageResult> package);

  /*
   * This function is mainly for the main interface
   * to update it's Chat Msg Related to a thread_id
   */
  void signal_update_chat_msg(std::shared_ptr<ChatMsgPageResult> package);

  /*
   * This function is mainly for create private chat UI widget
   * Server has already confirmed the behaviour
   * and returns a thread_id for this friend_uuid
   */
  void signal_create_private_chat(const QString &my_uuid,
                                  const QString &friend_uuid,
                                  const QString &thread_id);

private:
  /*establish tcp socket with server*/
  QTcpSocket m_chatting_server_socket;
  QTcpSocket m_resources_server_socket;

  /*create a connection buffer to store the data transfer from server*/
  RecvInfo m_chatting_info;
  RecvInfo m_resource_info;

  std::unique_ptr<RecvNodeType> m_chatting_buffer;
  std::unique_ptr<RecvNodeType> m_resources_buffer;

  /*according to service type to execute callback*/
  std::map<ServiceType, Callbackfunction> m_callbacks;
};

#endif // TCPNETWORKCONNECTION_H
