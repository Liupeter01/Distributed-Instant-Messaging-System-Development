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

public:
  bool updateVerificationStatus(const QString &unique_id,
                                const QString &msg_id) {

    if (!m_localMsgIndex.count(unique_id)) {
      qDebug() << "Unique ID Not Found!";
      return false;
    }

    auto it = m_localMsgIndex.find(unique_id);
    std::shared_ptr<T> msg = *(it->second);
    msg->setMsgID(msg_id);

    return insertVerified(msg) && removeLocal(unique_id);
  }

  [[nodiscard]]
  std::vector<std::shared_ptr<T>> dumpAllChatData() const {
    std::vector<std::shared_ptr<T>> ret;
    ret.reserve(m_verifyMessage.size() + m_localMessage.size());

    for (const auto &pair : m_verifyMessage)
      ret.push_back(pair.second);

    for (const auto &msg : m_localMessage)
      ret.push_back(msg);

    return ret;
  }

  bool insertMessage(std::shared_ptr<T> type) { return _insert(type); }

  bool _insert(std::shared_ptr<T> value) {

    if (value->isOnLocal()) {
      return insertLocal(value);
    }
    return insertVerified(value);
  }

  [[nodiscard]]
  std::vector<std::shared_ptr<T>> getLocalMessages() const {
    return {m_localMessage.begin(), m_localMessage.end()};
  }

  void clear() {
    m_localMessage.clear();
    m_localMsgIndex.clear();
    m_verifyMessage.clear();
    m_lastFetchedMsgId.clear();
  }

private:
  bool insertLocal(std::shared_ptr<T> msg) {
    const QString &id = msg->getUniqueId();
    if (m_localMsgIndex.count(id))
      return false;

    auto it = m_localMessage.insert(m_localMessage.end(), msg);
    m_localMsgIndex[id] = it;
    return true;
  }

  bool removeLocal(const QString &id) {
    if (!m_localMsgIndex.count(id))
      return false;

    auto it = m_localMsgIndex.find(id);
    m_localMessage.erase(it->second);
    m_localMsgIndex.erase(it);
    return true;
  }

  bool insertVerified(std::shared_ptr<T> value) {
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

    bool converted = false;
    const qint64 msgid = idStr.toLongLong(&converted, 10);
    if (!converted || msgid <= 0) {
        qDebug() << "Invalid msg_id! It should be a positive integer!";
        return false;
    }

    auto [it, inserted] = m_verifyMessage.try_emplace(msgid, std::move(value));
    if (!inserted) {
        qDebug() << "Msg ID already exists in verified messages!";
        return false;
    }

    m_lastFetchedMsgId = opt.value();
    return true;
  }

private:
  // Messages which only exist on local!
  std::list<std::shared_ptr<T>> m_localMessage;

  std::unordered_map<QString, typename decltype(m_localMessage)::iterator>
      m_localMsgIndex;

  // Messages which are verified by the server!
  std::map<
      /*msg_id*/ qint64,
      /*msg_data*/ std::shared_ptr<T>>
      m_verifyMessage;

  QString m_lastFetchedMsgId;
};

#endif // CHATMESSAGEBUFFER_H
