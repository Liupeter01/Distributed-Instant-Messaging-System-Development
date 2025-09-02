#ifndef CHATTINGDLGMAINFRAME_H
#define CHATTINGDLGMAINFRAME_H

#include <ChattingThreadDef.hpp>
#include <MsgNode.hpp>
#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <def.hpp>
#include <memory>
#include <unordered_map>

class SideBarWidget;
class QMouseEvent;
class QListWidgetItem;
class LoadingWaitDialog;
struct UserNameCard;
class UserFriendRequest;
struct ChattingTextMsg;
struct ChatThreadPageResult;

enum class MsgType;

namespace Ui {
class ChattingDlgMainFrame;
}

class ChattingDlgMainFrame : public QDialog {
  Q_OBJECT

  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;
  using ChattingBaseType = ChattingRecordBase;

public:
  explicit ChattingDlgMainFrame(QWidget *parent = nullptr);
  virtual ~ChattingDlgMainFrame();

public:
  static void sendHeartBeat();

signals:
  void switchToLogin();
  void signal_teminate_chatting_server(const QString &, const QString &);

protected:
  /*chat list test*/
  void addItemToChatListTest();

  /*customlized functions*/
  bool eventFilter(QObject *object, QEvent *event) override;

  /*windows switcher(accroding to m_dlgMode)*/
  void switchRelevantListWidget();

  /*register handler for chatting*/
  void updateMyChat();

  /*register handler for Contact list*/
  void updateMyContact();

  /*register handler for settings*/
  void updateMySettings();

  /*delegate sidebar widget*/
  void addLabel(SideBarWidget *widget);
  void resetAllLabels(SideBarWidget *new_widget);

  /*clear search_edit according to mouse position*/
  void clearSearchByMousePos(QMouseEvent *event);

  /* switch to chatting page by using stackedWidget */
  void switchChattingPage();

  /* switch to new user page by using stackedWidget */
  void switchNewUserPage();

  /*switch to user profile page by using stackedWidget*/
  void switchUserProfilePage();

  /*swith to user setting page*/
  void swithUserSettingPage();

  /*wait for remote server data*/
  void waitForDataFromRemote(bool status);

  /*
   * add QListWidgetItem to the list chattingThreadToUIWidget
   * and register current thread_id to m_chattingThreadToUIWidgetList
   */
  QListWidgetItem *addListWidgetItemToList(const QString &thread_id,
                                           std::shared_ptr<UserNameCard> info);

  /*load more chatting record*/
  void loadMoreChattingHistory();

private:
  void registerSignal();

  /*register action for search edit ui item*/
  void registerSearchEditAction();

  void registerSearchEditSignal();

  void updateSearchUserButton();

  void updateSideBarWidget(SideBarWidget *widget,
                           const QString &normal_pic_path,
                           const QString &hover_pic_path,
                           const QString &clicked_pic_path);

private slots:
  void slot_connection_status(bool status) { enable_heartBeart = status; }

  /*logout from server*/
  void slot_logout_status(bool status) { m_timer->stop(); }

  /*
   * waiting for data from remote server
   * status = true: activate
   * status = false: deactivate
   */
  void slot_waiting_for_data(bool status) { waitForDataFromRemote(status); }

  /*search text changed*/
  void slot_search_text_changed();

  /* load more limited items */
  void slot_load_more_contact_list();
  void slot_load_more_chatting_history();
  void slot_load_more_friending_requests();

  /*when side bar button activated, then display relevant info on show_list*/
  void slot_display_chat_list();
  void slot_display_contact_list();
  void slot_display_setting();
  /*
   * user click the item shown in the search list
   * 1. ListItemType::Default: DO NOTHING
   * 2. ListItemType::SearchUserId: When User Start To Searching User ID
   */
  void slot_search_list_item_clicked(QListWidgetItem *clicked_item);

  /*
   * user click the item shown in the chat list
   * 1. ListItemType::Default: DO NOTHING
   * 2. ListItemType::ChattingHistory: when user press chatting record
   */
  void slot_chat_list_item_clicked(QListWidgetItem *clicked_item);

  /*if target user has already became a auth friend with current user
   * then switch back to chatting dialog
   */
  void slot_switch_chat_item(std::shared_ptr<UserNameCard> info);

  /* switch to user profile page in stack widget*/
  void slot_switch_user_profile(std::shared_ptr<UserNameCard> info);

  /* switch to chatting page with friends info
   * But User Has to use special Data Structure to pass arguments
   */
  void slot_switch_chattingdlg_page(std::shared_ptr<UserChatThread> info);

  /*receive friend request from another user, this func is only for notifying
   * and storing request instance*/
  void slot_incoming_friend_request(
      std::optional<std::shared_ptr<UserFriendRequest>> info);

  /*
   * emit a signal to attach auth-friend messages to chatting history
   * This is the first offical chatting record,
   * so during this phase, "thread_id" will be dstributed to this chatting
   * thread!
   */
  void slot_add_auth_friend_init_chatting_thread(
      const UserChatType type, const QString &thread_id,
      std::shared_ptr<UserNameCard> namecard,
      std::vector<std::shared_ptr<FriendingConfirmInfo>> list);

  /*
   * although the messages which are sent will appear on the chattingstackpage
   * the message will not be recorded by the in the chattinghistory which is
   * stored by UserAccountManager
   */
  void slot_append_chat_message(const QString &thread_id,
                                std::shared_ptr<ChattingRecordBase> data);

  /**
   * @brief signal_update_local2verification_status
   * emit a signal to ChattingDlgMainFrame class to update local msg status
   * which returns an allocated msg_id to replace local uuid
   * @param thread_id
   * @param uuid
   * @param msg_id
   */
  void slot_update_local2verification_status(const QString &thread_id,
                                             const QString &uuid,
                                             const QString &msg_id);

  /*
   * sender sends chat msg to receiver
   * sender could be a user who is not in the chathistorywidget list
   * so we have to create a new widget for him
   */
  void slot_incoming_msg(MsgType msg_type,
                         std::shared_ptr<ChattingBaseType> msg);

  /*
   * This function is mainly for the main interface
   * to update it's chatting history UI widget
   */
  void slot_update_chat_thread(std::shared_ptr<ChatThreadPageResult> package);

  /*
   * This function is mainly for the main interface
   * to update it's Chat Msg Related to a thread_id
   */
  void slot_update_chat_msg(std::shared_ptr<ChatMsgPageResult> package);

  /*
   * This function is mainly for create private chat UI widget
   * Server has already confirmed the behaviour
   * and returns a thread_id for this friend_uuid
   */
  void slot_create_private_chat(const QString &my_uuid,
                                const QString &friend_uuid,
                                const QString &thread_id);

private:
  static bool enable_heartBeart;

  /*send heart beat package*/
  QTimer *m_timer;

  Ui::ChattingDlgMainFrame *ui;

  /*define how many chat recoreds are going to show up on chat record list*/
  static std::size_t CHATRECORED_PER_PAGE;
  std::size_t m_curr_chat_record_loaded = 0;

  /*reserve for search line edit*/
  QAction *m_searchAction;

  /*reserve for cancel user searching*/
  QAction *m_cancelAction;

  QVector<std::shared_ptr<SideBarWidget>> m_qlabelSet;

  /*
   * we use this to store all chatting thread
   * made by current user
   * std::unordered_map<QString, QListWidgetItem *> m_chatHistoryWidList; has
   * been deprecated!
   */
  std::unordered_map</*thread_id*/ QString,
                     /*widget item*/ QListWidgetItem *>
      m_chattingThreadToUIWidget;

  /*cur qlabel*/
  SideBarWidget *m_curQLabel;

  /*close status dialog*/
  std::shared_ptr<QDialog> m_Dlg;

  /*wait for remote server data status*/
  std::atomic<bool> m_send_status;

  /*LoadingWaitDialog*/
  std::shared_ptr<LoadingWaitDialog> m_loading;

  enum class ChattingDlgMode {
    ChattingDlgChattingMode,  // show multiple user chatting dialog
    ChattingDlgSearchingMode, // allow user to search
    ChattingDlgContactMode,   // show contacts
    ChattingDlgSettingMode    // User Setting
  } m_dlgMode;
};

#endif // CHATTINGDLGMAINFRAME_H
