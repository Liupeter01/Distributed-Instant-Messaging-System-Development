#include "resourcestoragemanager.h"

ResourceStorageManager::~ResourceStorageManager() {}

ResourceStorageManager::ResourceStorageManager() : m_info{} {}

void ResourceStorageManager::setUserInfo(std::shared_ptr<UserNameCard> info) {
  m_userInfo = info;
}

void ResourceStorageManager::recordUnfinishedTask(const QString &checksum,
                                                  std::shared_ptr<FileTransferDesc> info){

    if(!info)
        return;

    std::lock_guard<std::mutex> _lckg(m_mtx);

    auto it = m_unfinished_tasks.find(checksum);

    // insert new element
    if (it == m_unfinished_tasks.end()) {

        auto [_, status] =m_unfinished_tasks.try_emplace(checksum, info);
        if (!status)
            qDebug() << "key: checksum exist!\n";

        return;
    }

    // Update Only
    if (it->second) {
        it->second.reset();
    }
    it->second = info;
}

[[nodiscard]]
std::optional<std::shared_ptr<FileTransferDesc>>
ResourceStorageManager:: getUnfinishedTasks(const QString &str){
    std::lock_guard<std::mutex> _lckg(m_mtx);
    auto it = m_unfinished_tasks.find(str);
    if (it == m_unfinished_tasks.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ResourceStorageManager::removeUnfinishedTask(const QString &str){
     std::lock_guard<std::mutex> _lckg(m_mtx);
    auto it = m_unfinished_tasks.find(str);
    if (it == m_unfinished_tasks.end()) {
        return false;
    }
    m_unfinished_tasks.erase(it);
    return true;
}

bool ResourceStorageManager::isDownloading(const QString &str)
{
    if(auto opt = getUnfinishedTasks(str); opt ){
        return (*opt)->direction == TransferDirection::Download;
    }
    return false;
}

bool ResourceStorageManager::recordQLabelUpdateLists(const QString &path, QLabel *label)
{

    if(!label){
        return false;
    }


    auto s_ptr = std::shared_ptr<QLabel>(label, [](auto T){});

    auto it = m_batch_qlabels.find(path);
    if(it != m_batch_qlabels.end()){
        it->second.push_back(s_ptr);
        return true;
    }

    //create process
    auto [_, status] = m_batch_qlabels.try_emplace(path, std::vector<std::shared_ptr<QLabel>>{s_ptr});
    return status;
}

bool ResourceStorageManager::executeQLabelUpdateLists(const QString &path){

    auto it = m_batch_qlabels.find(path);
    if(it == m_batch_qlabels.end()){
        return false;
    }

     QPixmap pixmap(path);

    if( pixmap.isNull()){
        qDebug()<< "Pixmap Load Failed!\n";
        return false;
    }

    for(auto ib = it->second.begin() ; ib != it->second.end(); ib++){

        QPixmap pixmapScaled(pixmap.scaled((*ib)->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        (*ib)->setPixmap(pixmapScaled);
        (*ib)->setScaledContents(true);
        (*ib)->update();

    }

    m_batch_qlabels.erase(it);
}

void  ResourceStorageManager::removeQLabelUpdateLists(const QString &path)
{

    auto it = m_batch_qlabels.find(path);
    if(it != m_batch_qlabels.end()){

        m_batch_qlabels.erase(it);
    }

}
