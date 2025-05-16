#include "resourcestoragemanager.h"

ResourceStorageManager::~ResourceStorageManager() {}

ResourceStorageManager::ResourceStorageManager() : m_info{} {}

void ResourceStorageManager::setUserInfo(std::shared_ptr<UserNameCard> info) {
  m_userInfo = info;
}
