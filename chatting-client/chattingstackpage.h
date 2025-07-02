#ifndef CHATTINGSTACKPAGE_H
#define CHATTINGSTACKPAGE_H

#include <ChattingThreadDef.hpp>
#include <MsgNode.hpp>
#include <multiclickableqlabel.h>

namespace Ui {
class ChattingStackPage;
}

/*declaration*/
struct UserNameCard;
enum class MsgType;
enum class ChattingRole;
class ChattingMsgItem;

class ChattingStackPage : public QWidget {
  Q_OBJECT

  using SendNodeType = SendNode<QByteArray, ByteOrderConverterReverse>;
  using ChattingBaseType = ChattingRecordBase;

  void registerSignal();

  void handle_clicked(MultiClickableQLabel *label, const QString &hover,
                      const QString &clicked);

  void handle_hover(MultiClickableQLabel *label, const QString &click,
                    const QString &hover, const QString &normal);

public:
  explicit ChattingStackPage(QWidget *parent = nullptr);
  virtual ~ChattingStackPage();

public:
  /*
   * if this input uuid match to the friend we are chatting with
   * then we have to update the chattingdialog when its changed or updated
   */
  bool isThreadSwitchingNeeded(const QString &target_uuid) const;
  void switchChattingThread(std::shared_ptr<UserChatThread> user_thread);

protected:
  /*flush history and replace them with new one*/
  void setChattingThreadData(std::shared_ptr<UserChatThread> history);
  const ChattingRole getRole(std::shared_ptr<ChattingBaseType> value);

  std::optional<ChattingMsgItem *>
  setupChattingMsgItem(const ChattingRole role);

  void setupBubbleFrameOnItem(const ChattingRole role, const MsgType type,
                              ChattingMsgItem *item,
                              std::shared_ptr<ChattingBaseType> value);

private:
  static bool isChatValid(const MsgType type);
  void distribute(std::shared_ptr<ChattingBaseType> value);

signals:

  /*
   * expose specific chatting data to mainchattingdlg
   * chatting data could be added to the chattingthread
   */
  void signal_append_chat_data_on_local(MsgType msg_type,
                                        const QString &my_uuid,
                                        const QString &friend_uuid,
                                        const QJsonObject &obj);

private slots:
  void on_send_message_clicked();
  void on_clear_message_clicked();

private:
  Ui::ChattingStackPage *ui;

  std::size_t m_text_msg_counter;
  static std::size_t TXT_MSG_BUFFER_SIZE;

  /*target friend's info*/
  std::shared_ptr<UserNameCard> m_curFriendIdentity;
};

#endif // CHATTINGSTACKPAGE_H
