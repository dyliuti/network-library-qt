#include "util.h"

using namespace Net;
std::shared_ptr<GetTask> Util::getGetTask(const QString& url, const QJsonObject& obj)
{
    return createTask<GetTask>(url, obj);
}

std::shared_ptr<GetTask> Util::getGetTask(const QString& url, const QVariantMap& vm)
{
    return createTask<GetTask>(url, vm);
}

std::shared_ptr<PostUrlEncodeTask> Util::getPostByUrlEncodeTask(const QString& url, const QJsonObject& obj)
{
    return createTask<PostUrlEncodeTask>(url, obj);
}

std::shared_ptr<PostUrlEncodeTask> Util::getPostByUrlEncodeTask(const QString& url, const QVariantMap& vm)
{
    return createTask<PostUrlEncodeTask>(url, vm);
}

std::shared_ptr<PostJsonTask> Util::getPostByJsonTask(const QString& url, const QJsonObject& obj)
{
    return createTask<PostJsonTask>(url, obj);
}

std::shared_ptr<PostJsonTask> Util::getPostByJsonTask(const QString& url, const QVariantMap& vm)
{
    return createTask<PostJsonTask>(url, vm);
}

std::shared_ptr<PostMultiPartTask> Util::getPostByMultiPartTask(const QString& url, const QJsonObject& obj)
{
    return createTask<PostMultiPartTask>(url, obj);
}

std::shared_ptr<PostMultiPartTask> Util::getPostByMultiPartTask(const QString& url, const QVariantMap& vm)
{
    return createTask<PostMultiPartTask>(url, vm);
}

std::shared_ptr<DownloadTask> Util::getDownloadTask(const QString& url)
{
    return createTask<DownloadTask>(url);
}

std::shared_ptr<DownloadTask> Util::getDownloadTask(const QString& url, const QString& savePath)
{
    return createTask<DownloadTask>(url, savePath);
}

std::shared_ptr<UploadTask> Util::getUploadTask(const QString& url, const QJsonObject& obj, const std::vector<UploadResourceParamPtr>& resourceParams)
{
    return createTask<UploadTask>(url, obj, resourceParams);
}

std::shared_ptr<UploadTask> Util::getUploadTask(const QString& url, const QJsonObject& obj, const UploadResourceParamPtr& resourceParam)
{
    return createTask<UploadTask>(url, obj, resourceParam);
}

Util::Util()
    : QObject(nullptr)
{
}

void Util::removeTask(const QAtomicInteger<qint64>& taskId)
{
    QObject obj;
    connect(
        &obj, &QObject::destroyed, this, [=]() {
            std::unique_lock lock = std::unique_lock(m_taskMutex);
            for (int i = 0; i < m_tasks.size(); ++i) {
                if (m_tasks[i]->getTaskId() == taskId) {
                    m_tasks.removeAt(i);
                    break;
                }
            }
        },
        Qt::QueuedConnection);
}
