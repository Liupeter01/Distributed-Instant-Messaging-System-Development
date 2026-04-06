#ifndef CHATTINGTCPNETWORK_H
#define CHATTINGTCPNETWORK_H

#include "tcpnetworkbase.h"
#include <ChattingThreadDef.hpp>
#include <QObject>

struct UserNameCard;
struct UserFriendRequest;
struct ChatThreadPageResult;
enum class MsgType;

class ChattingTCPNetwork
    : public TCPNetworkBase,
      public Singleton<ChattingTCPNetwork>,
      public std::enable_shared_from_this<ChattingTCPNetwork> {
  Q_OBJECT
  friend class Singleton<ChattingTCPNetwork>;
  using ChattingBaseType = ChattingRecordBase;

public:
  virtual ~ChattingTCPNetwork() = default;

private:
  explicit ChattingTCPNetwork();

protected:
  void registerNetworkEvent() override;
  void registerCallback() override;
  void registerMetaType() override;

signals:
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

private slots:
  void slot_terminate_server() override;
  void slot_connect2_server() override;
};

#endif // CHATTINGTCPNETWORK_H
