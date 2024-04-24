#ifndef NETWORK_UPLOAD_TASK_H
#define NETWORK_UPLOAD_TASK_H
#include "posttask.h"
#include <QImage>
#include <QPixmap>
#include <memory>

namespace Net {
struct NETWORK_EXPORT UploadResourceParam : public std::enable_shared_from_this<UploadResourceParam> {
    UploadResourceParam(const QString& name, const QString& filename, const QString& type);
    virtual bool isEmpty() = 0;
    virtual QByteArray getByteArray() = 0;
    QString fileKey;
    QString fileName;
    // multipart/form-data; image/jpeg;  application/octet-stream
    QString contentType;
};
typedef std::shared_ptr<UploadResourceParam> UploadResourceParamPtr;

/*** 通过文件路径上传 ***/
struct NETWORK_EXPORT UploadFilePathParam : public UploadResourceParam {
    using UploadResourceParam::UploadResourceParam;
    bool isEmpty() override { return m_resource.isEmpty(); }
    QByteArray getByteArray() override;
    UploadResourceParamPtr setResource(const QString& filePath);

protected:
    QString m_resource;
};

/*** 通过raw字节上传 ***/
struct NETWORK_EXPORT UploadByteArrayParam : public UploadResourceParam {
    using UploadResourceParam::UploadResourceParam;
    bool isEmpty() override { return m_resource.isEmpty(); }
    QByteArray getByteArray() override;
    UploadResourceParamPtr setResource(const QByteArray& bytes);

protected:
    QByteArray m_resource;
};

/*** 上传pixmap ***/
struct NETWORK_EXPORT UploadPixmapParam : public UploadResourceParam {
    using UploadResourceParam::UploadResourceParam;
    bool isEmpty() override { return m_resource.isNull(); }
    QByteArray getByteArray() override;
    UploadResourceParamPtr setResource(const QPixmap& pix, const QString& format = QString());
    UploadResourceParamPtr setResource(QPixmap&& pix, const QString& format = QString());

protected:
    QPixmap m_resource;
    QString m_format;
};

/*** 上传QIMage，getByteArray还未实现。QImage转换到Byte有不同方式，应该还需额外一个参数来控制转换方式 ***/
// struct NETWORK_EXPORT UploadImageParam : public UploadResourceParam<QImage>
//{
//     using UploadResourceParam<QImage>::UploadResourceParam;
//     bool isEmpty() override { return resource.isNull(); }
//     QByteArray getByteArray() override;
// };

class NETWORK_EXPORT UploadTask : public PostMultiPartTask {
    Q_OBJECT
public:
    UploadTask(const QString& url, const QJsonObject& params, const std::vector<UploadResourceParamPtr>& resourceParams);
    UploadTask(const QString& url, const QJsonObject& params, const UploadResourceParamPtr& resourceParams);
    UploadTask& setThreadPoolEnable(bool enable);

signals:
    void sigUploadProgress(qint64 bytesSent, qint64 bytesTotal);

protected:
    QNetworkReply* execute() override;

protected:
    void addResourceMultiPart(const std::unique_ptr<QHttpMultiPart>& multiPart, const UploadResourceParamPtr& resourceParam);

private:
    std::vector<UploadResourceParamPtr> m_resourceParams;
    bool m_threadPoolEnable = true;
};
}
#endif // NETWORK_UPLOAD_TASK_H
