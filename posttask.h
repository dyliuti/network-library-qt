#ifndef NETWORK_POST_TASK_H
#define NETWORK_POST_TASK_H
#include "task.h"
#include <QHttpMultiPart>

namespace Net {
/***  Post请求结果 ***/
class NETWORK_EXPORT PostResult : public Result {
public:
    bool isSuccess() override;
    QString errorMsg(const QString& customErrorMsg) override;
};

/***  Post请求基类 ***/
class NETWORK_EXPORT PostTask : public Task {
    Q_OBJECT
public:
    PostTask(const QString& url, const QJsonObject& obj);
    PostTask(const QString& url, const QVariantMap& vm);
    // 默认开启，设置为false后还会打印url、参数、结果。但不会打印输出json结果等可能很长的结果内容
    // 若是很长的json结果, 上传代码时推荐设置为false，不输出json内容
    PostTask& setLongLogEnable(bool enable);

protected:
    ResultPtr createResult() override;
    void printResultLog(const ResultPtr& result) override;

protected:
    QJsonObject m_params;
    bool m_longLogEnable = true;
};

/*** application/x-www-form-urlencoded Post请求 ***/
class NETWORK_EXPORT PostUrlEncodeTask : public PostTask {
    Q_OBJECT
    using PostTask::PostTask;

protected:
    QNetworkReply* execute() override;
    QString getContentType() override;
};

/*** application/json Post请求 ***/
class NETWORK_EXPORT PostJsonTask : public PostTask {
    Q_OBJECT
    using PostTask::PostTask;

protected:
    QNetworkReply* execute() override;
    QString getContentType() override;
};

/*** multipart Post请求 ***/
class NETWORK_EXPORT PostMultiPartTask : public PostTask {
    Q_OBJECT
    using PostTask::PostTask;

protected:
    QNetworkReply* execute() override;
    QString getContentType() override;

protected:
    void addHttpParamPart(const std::unique_ptr<QHttpMultiPart>& multiPart, const QJsonObject& params);

protected:
    std::unique_ptr<QHttpMultiPart> m_multiPart;
};
}
#endif // NETWORK_POST_TASK_H
