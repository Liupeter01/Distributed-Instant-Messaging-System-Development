#pragma once
#ifndef _CHATTINGTHREADDEF_H
#define _CHATTINGTHREADDEF_H
#include <optional>
#include <string>
#include <UserDef.hpp>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <useraccountmanager.hpp>
#include <ChattingMessageBuffer.hpp>

//Chatting Type
enum class UserChatType {
    PRIVATE, GROUP
};

enum class MsgType{
    DEFAULT, TEXT, IMAGE, FILE, AUDIO, VIDEO
};

/*
 * ThreadInfo is a TEMP structure aiming to store the the relationship of
 * thread_id <--Private Chat-> (user_one, user_two)
 * thread_id <--Group Chat-> (None)
 */
struct ChatThreadMeta {
    ChatThreadMeta() = default;
          ChatThreadMeta(const std::string& __thread_id,
                                      const UserChatType _type,
                                        std::optional<std::string> one = std::nullopt,
                                        std::optional<std::string> two = std::nullopt)
                    :_thread_id(__thread_id), _chat_type(_type), _user_one(one), _user_two(two)
          {}

          bool isGroupChat() const {
                    return _chat_type == UserChatType::GROUP;
          }

          static ChatThreadMeta createPrivateChat(const std::string& thread_id,
                                                  const std::string& user_one,
                                                  const std::string& user_two) {
              return ChatThreadMeta(thread_id, UserChatType::PRIVATE, user_one, user_two);
          }

          static ChatThreadMeta createGroupChat(const std::string& thread_id) {
              return ChatThreadMeta(thread_id, UserChatType::GROUP);
          }

          std::string _thread_id;
          UserChatType _chat_type;

          //those two variables are for private chat, if its a group chat then ignore them
          std::optional<std::string> _user_one = std::nullopt;
          std::optional<std::string> _user_two = std::nullopt;
};


struct ChatThreadPageResult{
    ChatThreadPageResult() = default;
    ChatThreadPageResult(const bool load_more,
                         const QString &next_id,
                         std::vector<std::unique_ptr<ChatThreadMeta> > &&lists)
        :m_load_more(load_more)
        ,m_next_thread_id(next_id)
        ,m_lists(std::move(lists))
    {
    }

    //any more data?
    bool m_load_more = false;

    //if so, whats the next thread_id we are going to use in next round query!
    QString m_next_thread_id;

    std::vector<std::unique_ptr<ChatThreadMeta>> m_lists;
};

struct ChattingThreadDesc {

    ChattingThreadDesc() = default;
    ChattingThreadDesc(const ChatThreadMeta& o)
        :_meta(o._thread_id, o._chat_type, o._user_one, o._user_two)
        , _last_msg_id(std::nullopt)
    {
    }

    ChattingThreadDesc(const std::string& __thread_id,
                       const UserChatType _type,
                       std::optional < std::string> __last_msg_id = std::nullopt,
                       std::optional<std::string> one = std::nullopt,
                       std::optional<std::string> two = std::nullopt)
        :_meta(__thread_id, _type, one, two)
        , _last_msg_id(__last_msg_id)
    {
    }

    bool isGroupChat() const {
        return _meta.isGroupChat();
    }
    const std::string& getThreadId()const {
        return _meta._thread_id;
    }
    std::optional < std::string> getLastMsgId() const {
        return _last_msg_id;
    }
    std::optional < std::string> getUserOneUUID() const{
        return _meta._user_one;
    }
    std::optional < std::string> getUserTwoUUID() const {
        return _meta._user_two;
    }

    void setLastMsgId(const std::string&last){
        _last_msg_id = last;
    }

    static ChattingThreadDesc createPrivateChat(const std::string& thread_id,
                                                const std::string& user_one,
                                                const std::string& user_two) {
        return ChattingThreadDesc(thread_id, UserChatType::PRIVATE, std::nullopt, user_one, user_two);
    }

    static ChattingThreadDesc createGroupChat(const std::string& thread_id) {
        return ChattingThreadDesc(thread_id, UserChatType::GROUP);
    }


    ChatThreadMeta _meta;
    std::optional < std::string> _last_msg_id;     //last msg id for data retrieve!
};

struct FriendingConfirmInfo {
    FriendingConfirmInfo() = default;

    FriendingConfirmInfo(const MsgType type,
                         const QString& threadId,
                         const QString& messageId,
                         const QString& sender,
                         const QString& receiver,
                         const QString& content)

        : message_type(type),
        thread_id(threadId),
        message_id(messageId),
        message_sender(sender),
        message_receiver(receiver),
        message_content(content) {}

    MsgType message_type;
    QString thread_id;
    QString message_id;
    QString message_sender;
    QString message_receiver;
    QString message_content;
};

    struct ChattingRecordBase{
        ChattingRecordBase() = default;

        //Generate Uniqueid by default
        ChattingRecordBase(const QString &sender,
                           const QString &receiver,
                           MsgType _type = MsgType::DEFAULT)

            : sender_uuid(sender)
            , receiver_uuid(receiver)
            , type(_type)
            , m_uniqueId(QUuid::createUuid().toString())
        {}

        //User input unique_id!
        ChattingRecordBase(const QString &sender,
                           const QString &receiver,
                           const QString &unique_id,
                           MsgType _type = MsgType::DEFAULT)

            : sender_uuid(sender)
            , receiver_uuid(receiver)
            , type(_type)
            , m_uniqueId(unique_id)
        {}

        virtual void setMsgID(const QString& value) = 0;
        virtual std::optional<QString> getMsgID() = 0;
        virtual const QString&  unsafe_getMsgID() const  = 0;

        virtual const QString& getMsgContent() = 0;
        const MsgType getMsgType() const {return type; }
        const QString &getUniqueId() const { return m_uniqueId; }
        bool isOnLocal() const {return m_status;}

protected:
        void setVerified(bool status){ m_status = status; }

public:
        MsgType type = MsgType::DEFAULT;
        QString sender_uuid;
        QString receiver_uuid;

        /*unique_id was generate by default, for data transmission status use!*/
        bool m_status = false;
        QString m_uniqueId;
    };

    struct ChattingTextMsg : public ChattingRecordBase{

      ChattingTextMsg(const QString &sender,
                      const QString &receiver)
            :  ChattingRecordBase(sender, receiver, MsgType::TEXT)
        {}

      ChattingTextMsg(const QString &sender, const QString &receiver,
                      const QString &_msg_content)
          :ChattingRecordBase(sender, receiver, MsgType::TEXT)
          , m_data(std::make_unique<UserChatTextRecord>("", _msg_content))
      {
      }

      ChattingTextMsg(const QString &sender, const QString &receiver,
                      const QString &unique_id, const QString &_msg_content)
           :ChattingRecordBase(sender, receiver, unique_id, MsgType::TEXT)
          , m_data(std::make_unique<UserChatTextRecord>("", _msg_content))
       {
       }
        std::optional<QString> getMsgID() override{
            if(isOnLocal()){
                return std::nullopt;
            }
            return m_data->m_msg_id;
       }
       const QString& getMsgContent() override{
           return m_data->m_msg_content;
       }
       void setMsgID(const QString& value) override{
           m_data->m_msg_id.clear();
           m_data->m_msg_id = value;
           this->setVerified(true);
       }

       const QString& unsafe_getMsgID() const override{
           return m_data->m_msg_id;
       }

       struct UserChatTextRecord {
           UserChatTextRecord() = default;
           UserChatTextRecord(const QString &id, const QString &msg)
               : m_msg_id(id), m_msg_content(msg) {}

           QString m_msg_id;
           QString m_msg_content;
       };

       std::unique_ptr<UserChatTextRecord> m_data;
    };

    struct ChattingVoice : public ChattingRecordBase{
      ChattingVoice(const QString &sender, const QString &receiver)
            : ChattingRecordBase(sender, receiver, MsgType::AUDIO) {}

      std::optional<QString> getMsgID() override{
          if(isOnLocal()){
              return std::nullopt;
          }
          return m_msg_id;
      }
      const QString& getMsgContent() override{
          return m_msg_content;
      }
      void setMsgID(const QString& value) override{
          m_msg_id = value;
          this->setVerified(true);
      }
      const QString& unsafe_getMsgID() const override{
          return m_msg_id;
      }

      QString m_msg_id;
      QString m_msg_content;
    };

    struct ChattingVideo : public ChattingRecordBase{
      ChattingVideo(const QString &sender, const QString &receiver)
            : ChattingRecordBase(sender, receiver, MsgType::VIDEO) {}

      std::optional<QString> getMsgID() override{
          if(isOnLocal()){
              return std::nullopt;
          }
          return m_msg_id;
      }
      const QString& unsafe_getMsgID() const override{
          return m_msg_id;
      }
      const QString& getMsgContent() override{
          return m_msg_content;
      }
      void setMsgID(const QString& value) override{
          m_msg_id = value;
          this->setVerified(true);
      }

      QString m_msg_id;
      QString m_msg_content;
    };

    struct ChattingImage : public ChattingRecordBase{
      ChattingImage(const QString &sender, const QString &receiver)
            : ChattingRecordBase(sender, receiver, MsgType::IMAGE) {}

      std::optional<QString> getMsgID() override{
          if(isOnLocal()){
              return std::nullopt;
          }
          return m_msg_id;
      }
      const QString& unsafe_getMsgID() const override{
          return m_msg_id;
      }
      const QString& getMsgContent() override{
          return m_msg_content;
      }
      void setMsgID(const QString& value) override{
          m_msg_id = value;
          this->setVerified(true);
      }

      QString m_msg_id;
      QString m_msg_content;
    };

    struct ChattingFile : public ChattingRecordBase{
      ChattingFile(const QString &sender, const QString &receiver)
            : ChattingRecordBase(sender, receiver, MsgType::FILE) {}

      std::optional<QString> getMsgID() override{
          if(isOnLocal()){
              return std::nullopt;
          }
          return m_msg_id;
      }
      const QString& getMsgContent() override{
          return m_msg_content;
      }
      const QString& unsafe_getMsgID() const override{
          return m_msg_id;
      }
      void setMsgID(const QString& value) override{
          m_msg_id = value;
          this->setVerified(true);
      }

      QString m_msg_id;
      QString m_msg_content;
    };

    template <typename _Type>
    using check_datatype_v = typename std::enable_if<
        std::is_same_v<ChattingRecordBase, std::decay_t<_Type>> ||
            std::is_same_v<ChattingTextMsg, std::decay_t<_Type>> ||
            std::is_same_v<ChattingVoice, std::decay_t<_Type>> ||
            std::is_same_v<ChattingVideo, std::decay_t<_Type>>,
        int>::type;

    template <typename _Ty>
    static constexpr bool allowed_types = std::is_same_v<ChattingRecordBase, std::decay_t<_Ty>> ||
                                          std::is_same_v<ChattingTextMsg, _Ty> ||
                                          std::is_same_v<ChattingVoice, _Ty> ||
                                          std::is_same_v<ChattingVideo, _Ty>;

/*store the friend's identity and the historical info sent before*/
class UserChatThread {
public:
    using ChatType = ChattingRecordBase;

    explicit UserChatThread(const QString &thread_id,
                            const UserNameCard &card,
                            const UserChatType type = UserChatType::PRIVATE)

        : m_threadId(thread_id)
        , m_peerCard(std::make_shared<UserNameCard>(card))
        , m_userChatType(type)
        , m_recordStorge()
    {}

public:
    void setCurChattingThreadId(const QString& thread_id){
        m_threadId = thread_id;
    }

    const
    QString &
    getCurChattingThreadId() const {
        return m_threadId;
    }

    std::shared_ptr<UserNameCard>
    getUserNameCard() {
        return m_peerCard;
    }

    const
    UserChatType
    getUserChatType() const{ return m_userChatType; }

    bool
    insertMessage(std::shared_ptr<ChatType> msg) {
        return m_recordStorge.insertMessage(msg);
    }

    [[nodiscard]]
    std::vector<std::shared_ptr<ChatType>>
    dumpAll() const {
        return m_recordStorge.dumpAllChatData();
    }

    [[nodiscard]]
    std::vector<std::shared_ptr<ChatType>>
    getLocalMessages() const {
        return m_recordStorge.getLocalMessages();
    }

    bool
    promoteLocalToVerified(const QString& unique_id,
                           const QString& msg_id) {
        return m_recordStorge.updateVerificationStatus(unique_id, msg_id);
    }

    [[nodiscard]]
    static
    std::shared_ptr<ChatType>
    generatePackage(const FriendingConfirmInfo& info){
        std::shared_ptr<ChatType> res;

        if(info.message_type == MsgType::TEXT){
            res = std::make_shared<ChattingTextMsg>(
                info.message_sender,
                info.message_receiver,
                info.message_content
            );
            res->setMsgID(info.message_id);
        }
        else if(info.message_type == MsgType::AUDIO){}
        else if(info.message_type == MsgType::IMAGE){}

        return res;
    }

    [[nodiscard]]
    static
    std::shared_ptr<ChatType>
    generatePackage(const MsgType type, const QJsonObject &obj){

        std::shared_ptr<ChatType> res;

        auto sender = obj["msg_sender"].toString();
        auto receiver = obj["msg_receiver"].toString();
        auto unique_id = obj["unique_id"].toString();

        if(type == MsgType::TEXT){
            res = std::make_shared<ChattingTextMsg>(
                sender,
                receiver,
                unique_id,
                obj["msg_content"].toString()
            );
        }
        else if(type == MsgType::AUDIO){}
        else if(type == MsgType::IMAGE){}

        return res;
    }

private:
    /*thread_id*/
    QString m_threadId;

    //prepared for pulling data from server! ASC by default!
    QString m_lastFetchedMsgId;

    //chat type
    UserChatType m_userChatType;

    //last message
    std::shared_ptr<ChatType> m_lastMessage;

    /* For private and group chat part, maybe we could use variant in the future */

    //PRIVATE CHAT ONLY -- friend uuid
    std::shared_ptr<UserNameCard> m_peerCard;

    //GROUP CHAT ONLY
    //???

    //Message storge structure
    MessageBuffer<ChatType> m_recordStorge;
};

#endif //_CHATTINGTHREADDEF_H
