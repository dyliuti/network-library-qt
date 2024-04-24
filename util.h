#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H

#include "downloadtask.h"
#include "gettask.h"
#include "network_global.h"
#include "posttask.h"
#include "task.h"
#include "uploadtask.h"
#include <QAtomicInteger>
#include <QObject>
#include <functional>
#include <mutex>

namespace Net {
// 调用说明文档：InstructionForUse.h
class NETWORK_EXPORT Util : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Util)

public:
    std::shared_ptr<GetTask> getGetTask(const QString& url, const QJsonObject& obj);
    std::shared_ptr<GetTask> getGetTask(const QString& url, const QVariantMap& vm);
    std::shared_ptr<PostUrlEncodeTask> getPostByUrlEncodeTask(const QString& url, const QJsonObject& obj);
    std::shared_ptr<PostUrlEncodeTask> getPostByUrlEncodeTask(const QString& url, const QVariantMap& vm);
    std::shared_ptr<PostJsonTask> getPostByJsonTask(const QString& url, const QJsonObject& obj);
    std::shared_ptr<PostJsonTask> getPostByJsonTask(const QString& url, const QVariantMap& vm);
    std::shared_ptr<PostMultiPartTask> getPostByMultiPartTask(const QString& url, const QJsonObject& obj);
    std::shared_ptr<PostMultiPartTask> getPostByMultiPartTask(const QString& url, const QVariantMap& vm);
    /*** 下载到缓存，小文件可以，注意文件大小不要超过500M ***/
    std::shared_ptr<DownloadTask> getDownloadTask(const QString& url);
    /*** 下载到本地文件 ***/
    std::shared_ptr<DownloadTask> getDownloadTask(const QString& url, const QString& savePath);
    std::shared_ptr<UploadTask> getUploadTask(const QString& url, const QJsonObject& obj, const std::vector<UploadResourceParamPtr>& resourceParams);
    std::shared_ptr<UploadTask> getUploadTask(const QString& url, const QJsonObject& obj, const UploadResourceParamPtr& resourceParam);

    static Util& instance()
    {
        static Util self;
        return self;
    }

private:
    Util();
    template <typename T, typename... U>
    std::shared_ptr<T> createTask(U const&... args)
    {
        auto task = std::make_shared<T>(args...);
        task->setTaskId(m_nextAllocTaskId++);
        m_taskMutex.lock();
        m_tasks.push_back(task);
        m_taskMutex.unlock();
        connect(task.get(), &T::sigTaskOver, this, [this](ResultPtr result) {
            this->removeTask(result->getTaskId());
        });

        return task;
    }

    void removeTask(const QAtomicInteger<qint64>& taskId);

private:
    QAtomicInteger<qint64> m_nextAllocTaskId = 0;
    QList<std::shared_ptr<Task>> m_tasks;
    std::mutex m_taskMutex;
};
}
#endif // NETWORK_UTIL_H
