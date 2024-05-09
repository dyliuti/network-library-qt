#include "posttask.h"
#include <QJsonDocument>

using namespace Net;
/*** Post结果 ***/
bool PostResult::isSuccess()
{
    return networkSuccess() /*** && customize success judgmenets  ***/;
}

QString PostResult::errorMsg(const QString& customErrorMsg)
{
    // customize error msg
    return "";
}

/*** Post请求基类 ***/
PostTask::PostTask(const QString& url, const QJsonObject& obj)
    : Task(url)
{
    qInfo() << QStringLiteral("Task begin, params: ") << obj;
    m_params = convetJsonValueToString(obj);
}

PostTask::PostTask(const QString& url, const QVariantMap& vm)
    : PostTask(url, QJsonObject::fromVariantMap(vm))
{
}

PostTask& PostTask::setLongLogEnable(bool enable)
{
    m_longLogEnable = enable;
    return *this;
}

ResultPtr PostTask::createResult()
{
    return std::make_shared<PostResult>();
}

void PostTask::printResultLog(const ResultPtr& result)
{
    if (m_longLogEnable) {
        qInfo() << QStringLiteral("Task finished, isSuccess: %1, errorMsg: %2, httpCode: %4, handle result elapsedTime: %5ms;").arg(result->isSuccess() ? "true" : "false").arg(result->errorMsg()).arg(result->m_httpCode).arg(m_elapsedTimer.elapsed())
                << "result: " << result->getJsonObject();
    } else {
        qInfo() << QStringLiteral("Task finished, isSuccess: %1, errorMsg: %2, httpCode: %4, handle result elapsedTime: %5ms").arg(result->isSuccess() ? "true" : "false").arg(result->errorMsg()).arg(result->m_httpCode).arg(m_elapsedTimer.elapsed());
    }
}

/*** Post application/json请求 ***/
QNetworkReply* PostJsonTask::execute()
{
    return getNetworkAccessManager()->post(m_request, QJsonDocument(m_params).toJson());
}

QString PostJsonTask::getContentType()
{
    return QStringLiteral("application/json");
}

#include <QUrlQuery>
/*** Post application/x-www-form-urlencoded请求 ***/
QNetworkReply* PostUrlEncodeTask::execute()
{
    QUrlQuery postData;
    QList<QPair<QString, QString>> query;
    for (auto it = m_params.constBegin(); it != m_params.constEnd(); ++it) {
        query.push_back(QPair<QString, QString>(it.key(), QUrl::toPercentEncoding(it.value().toString())));
    }
    postData.setQueryItems(query);
    return getNetworkAccessManager()->post(m_request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

QString PostUrlEncodeTask::getContentType()
{
    return QStringLiteral("application/x-www-form-urlencoded");
}

#include <QHttpPart>
/*** Post application/form-data请求 ***/
QNetworkReply* PostMultiPartTask::execute()
{
    m_multiPart = std::make_unique<QHttpMultiPart>(QHttpMultiPart::FormDataType);
    addHttpParamPart(m_multiPart, m_params);

    return getNetworkAccessManager()->post(m_request, m_multiPart.get());
}

QString PostMultiPartTask::getContentType()
{
    return QStringLiteral("");
}

void PostMultiPartTask::addHttpParamPart(const std::unique_ptr<QHttpMultiPart>& multiPart, const QJsonObject& params)
{
    const auto& keys = params.keys();
    for (const auto& key : keys) {
        const auto& value = params.value(key).toString();
        QHttpPart httpPart;
        httpPart.setHeader(QNetworkRequest::ContentDispositionHeader, QString(R"(form-data; name="%1")").arg(key));
        httpPart.setBody(value.toUtf8());
        multiPart->append(httpPart);
    }
}
