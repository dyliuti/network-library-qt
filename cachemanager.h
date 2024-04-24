#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include "network_global.h"
#include <QObject>
#include <mutex>

namespace Net {
class NETWORK_EXPORT CacheManager : public QObject {
    Q_OBJECT
public:
    static CacheManager& instance();

    QString getCacheDirectory(bool usingOneFolder = false, const QString& path = "");

    void setLockTag(quint64 uid);
    void setClearTag(quint64 uid);

protected:
    CacheManager();
    ~CacheManager();

    CacheManager(CacheManager const&) = delete;
    CacheManager(CacheManager&&) = delete;
    CacheManager& operator=(CacheManager const&) = delete;
    CacheManager& operator=(CacheManager&&) = delete;

private:
    static void clearCache();

private:
    static std::mutex _mutex;
};
}
#endif // CACHEMANAGER_H
