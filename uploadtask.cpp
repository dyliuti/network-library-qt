#include "uploadtask.h"
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QThreadPool>

using namespace Net;
UploadResourceParam::UploadResourceParam(const QString& name, const QString& filename, const QString& type)
    : fileKey(name)
    , fileName(filename)
    , contentType(type)
{
}

QByteArray UploadFilePathParam::getByteArray()
{
    auto file = std::make_unique<QFile>(m_resource);
    if (!file->exists()) {
        return "";
    }
    if (!file->open(QIODevice::ReadOnly))
        return "";

    return file->readAll();
}

UploadResourceParamPtr UploadFilePathParam::setResource(const QString& filePath)
{
    m_resource = filePath;
    return shared_from_this();
}

QByteArray UploadByteArrayParam::getByteArray()
{
    return m_resource;
}

UploadResourceParamPtr UploadByteArrayParam::setResource(const QByteArray& bytes)
{
    m_resource = bytes;
    return shared_from_this();
}

QByteArray UploadPixmapParam::getByteArray()
{
    QByteArray picBytes;
    QBuffer buffer(&picBytes);
    buffer.open(QIODevice::WriteOnly);
    m_resource.save(&buffer, m_format.toLocal8Bit());

    return buffer.buffer();
}

UploadResourceParamPtr UploadPixmapParam::setResource(const QPixmap& pix, const QString& format)
{
    m_resource = pix;
    m_format = format;
    return shared_from_this();
}

UploadResourceParamPtr UploadPixmapParam::setResource(QPixmap&& pix, const QString& format)
{
    m_resource = std::move(pix);
    m_format = format;
    return shared_from_this();
}

// Todo: 将QImage转换为QByteArray
// QByteArray UploadImageParam::getByteArray()
//{
//    return QByteArray();
//}

/*** Upload请求 ***/
UploadTask::UploadTask(const QString& url, const QJsonObject& params, const std::vector<UploadResourceParamPtr>& resourceParams)
    : PostMultiPartTask(url, params)
    , m_resourceParams(resourceParams)
{
    m_timeout = 0;
}

UploadTask::UploadTask(const QString& url, const QJsonObject& params, const UploadResourceParamPtr& resourceParam)
    : PostMultiPartTask(url, params)
{
    m_timeout = 0;
    m_resourceParams.emplace_back(resourceParam);
}

UploadTask& UploadTask::setThreadPoolEnable(bool enable)
{
    m_threadPoolEnable = enable;
    return *this;
}

QNetworkReply* UploadTask::execute()
{
    m_multiPart = std::make_unique<QHttpMultiPart>(QHttpMultiPart::FormDataType);
    auto threadFunc = [=]() {
        addHttpParamPart(m_multiPart, m_params);
        for (const auto& resourseParam : m_resourceParams) {
            addResourceMultiPart(m_multiPart, resourseParam);
        }

        QMetaObject::invokeMethod(
            this, [=]() {
                m_networkReply = getNetworkAccessManager()->post(m_request, m_multiPart.get());
                connect(m_networkReply, &QNetworkReply::uploadProgress, this, &UploadTask::sigUploadProgress);
                connect(m_networkReply, &QNetworkReply::finished, this, &UploadTask::onRequestFinished);
                connect(m_networkReply, &QNetworkReply::sslErrors, this, &UploadTask::onCopeSslErrors);
            },
            Qt::QueuedConnection);
    };

    if (m_threadPoolEnable) {
        QThreadPool::globalInstance()->start(new Runnable(threadFunc));
    } else {
        threadFunc();
    }

    return nullptr;
}

void UploadTask::addResourceMultiPart(const std::unique_ptr<QHttpMultiPart>& multiPart, const UploadResourceParamPtr& resourceParam)
{
    if (resourceParam->isEmpty()) {
        return;
    }

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, resourceParam->contentType);
    QString header = QString(R"(form-data; name="%1"; filename="%2")").arg(resourceParam->fileKey).arg(resourceParam->fileName);
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, header);

    filePart.setBody(resourceParam->getByteArray());

    multiPart->append(filePart);
}
