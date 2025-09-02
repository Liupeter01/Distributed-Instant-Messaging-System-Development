#pragma once
#ifndef CHATMESSAGEBUFFER_H
#define CHATMESSAGEBUFFER_H
#include <QDebug>
#include <QString>
#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

template <typename T> class MessageBuffer {
public:
  MessageBuffer() = default;
  virtual ~MessageBuffer() = default;

  using LinkListNode = std::shared_ptr<T>;
  using LinkList = std::list<std::shared_ptr<T>>;
  using LinkListNodePtr = typename LinkList::iterator;

public:
  bool updateVerificationStatus(const QString &unique_id,
                                const QString &msg_id) {

    if (msg_id.isEmpty() || unique_id.isEmpty()) {
      qDebug() << "Empty msg_id or unique_id!";
      return false;
    }

    auto it = m_localMsgIndex.find(unique_id);
    if (it == m_localMsgIndex.end()) {
      qDebug() << "Unique ID Not Found!";
      return false;
    }

    if (!m_verifyMessage.count(msg_id)) {
      qDebug() << "Msg ID already exists in verified messages!";
      return false;
    }

    auto node_it = it->second;
    auto &msg = *node_it;
    if (!msg) {
      qDebug() << "Null message pointer!";
      return false;
    }

    msg->setMsgID(msg_id);

    /* <msg_id, pointer> */
    auto [_, inserted] = m_verifyMessage.try_emplace(msg_id, node_it);
    if (!inserted) {
      qDebug() << "Msg ID already exists in verified messages!";
      return false;
    }

    // erase the local iterator pointer from the local unordered_map
    m_localMsgIndex.erase(it);
    return true;
  }

  [[nodiscard]]
  std::vector<std::shared_ptr<T>> dumpAllChatData() const {
    return {m_messages.begin(), m_messages.end()};
  }

  bool insertMessage(std::shared_ptr<T> type) { return _insert(type); }

  bool _insert(std::shared_ptr<T> value) {

    // put it inside linklist directly to ensure the sequences
    auto it = m_messages.insert(m_messages.end(), value);

    bool ok = value->isOnLocal() ? insertLocal(value->getUniqueId(), it)
                                 : insertVerified(value, it);

    // Rollback to ensure safty
    if (!ok) {
      qDebug() << "Rollback Happening!\n";
      m_messages.erase(it);
    }
    return ok;
  }

  void clear() {
    m_messages.clear();
    m_localMsgIndex.clear();
    m_verifyMessage.clear();
    m_lastFetchedMsgId.clear();
  }

private:
  bool insertLocal(const QString &unique_id, LinkListNodePtr pointer) {

    if (unique_id.isEmpty()) {
      qDebug() << "Empty munique_id!";
      return false;
    }

    if (m_localMsgIndex.count(unique_id))
      return false;

    /* <unique_id, pointer> */
    auto [it, inserted] = m_localMsgIndex.try_emplace(unique_id, pointer);
    if (!inserted) {
      qDebug() << "Unique Id already exists in local messages!";
      return false;
    }
    return true;
  }

  bool insertVerified(std::shared_ptr<T> value, LinkListNodePtr pointer) {
    if (!value) {
      qDebug() << "Null value!";
      return false;
    }

    auto opt = value->getMsgID();
    if (!opt.has_value())
      return false;

    const QString idStr = opt->trimmed();
    if (idStr.isEmpty()) {
      qDebug() << "Empty msg_id!";
      return false;
    }

    /* <msg_id, pointer> */
    auto [it, inserted] = m_verifyMessage.try_emplace(idStr, pointer);
    if (!inserted) {
      qDebug() << "Msg ID already exists in verified messages!";
      return false;
    }

    m_lastFetchedMsgId = opt.value();
    return true;
  }

private:
  // Messages which are stored in here!
  // local message iterator(pointer) is going to store in unordered_map
  // remote message iterator(pointer) is going to store in map
  LinkList m_messages;

  std::unordered_map<
      /*unique_id*/
      QString,
      /*the pointer to msg*/
      LinkListNodePtr>
      m_localMsgIndex;

  // Messages which are verified by the server!
  std::map<
      /*msg_id*/
      QString,
      /*the pointer to msg*/
      LinkListNodePtr>
      m_verifyMessage;

  QString m_lastFetchedMsgId;
};

#endif // CHATMESSAGEBUFFER_H
