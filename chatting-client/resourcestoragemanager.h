#ifndef RESOURCESTORAGEMANAGER_H
#define RESOURCESTORAGEMANAGER_H
#include <QString>
#include "singleton.hpp"
#include <UserNameCard.h>

class ResourceStorageManager
    : public Singleton<ResourceStorageManager> {
    friend class Singleton<ResourceStorageManager>;

public:
    ~ResourceStorageManager();
    void setUserInfo(std::shared_ptr<UserNameCard> info);

    void set_host(const QString &_host) { m_info.host = _host; }
    void set_port(const QString &_port) { m_info.port = _port; }
    //void set_token(const QString &_token) { m_info.token = _token; }
    void set_uuid(const QString &_uuid) { m_info.uuid = _uuid; }

    const QString &get_host() const { return m_info.host; }
    const QString &get_port() const { return m_info.port; }
    //const QString &get_token() const { return m_info.token; }
    const QString get_uuid() const { return m_info.uuid; }

private:
    ResourceStorageManager();

private:
    struct ResourcesServerInfo{
        ResourcesServerInfo()
            :uuid(), host(), port()
        {}

        QString uuid;
        QString host;
        QString port;
    } m_info;

    /*store current user's info*/
    std::shared_ptr<UserNameCard> m_userInfo;
};

#endif // RESOURCESTORAGEMANAGER_H
