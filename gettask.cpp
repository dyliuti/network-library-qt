#include "gettask.h"

using namespace Net;
bool GetResult::isSuccess()
{
    return networkSuccess();
}

QString GetResult::errorMsg(const QString& customErrorMsg)
{
    // custom error msg
    return "";
}

/*** Get请求 ***/
GetTask::GetTask(const QString& url)
    : Task(url)
{
}

GetTask::GetTask(const QString& url, const QJsonObject& obj)
    : Task(url)
{
    m_params = convetJsonValueToString(obj);
}

GetTask::GetTask(const QString& url, const QVariantMap& vm)
    : Task(url)
{
    m_params = convetJsonValueToString(QJsonObject::fromVariantMap(vm));
}

GetTask& GetTask::setLongLogEnable(bool enable)
{
    m_longLogEnable = enable;
    return *this;
}

QNetworkReply* GetTask::execute()
{
    for (auto it = m_params.constBegin(); it != m_params.constEnd(); ++it) {
        if (it == m_params.constBegin()) {
            m_url = QStringLiteral("%1?%2=%3").arg(m_url).arg(it.key()).arg(it.value().toString());
        } else {
            m_url = QStringLiteral("%1&%2=%3").arg(m_url).arg(it.key()).arg(it.value().toString());
        }
    }

    m_request.setUrl(QUrl(m_url));
    return getNetworkAccessManager()->get(m_request);
}

QString GetTask::getContentType()
{
    return QStringLiteral("");
}

ResultPtr GetTask::createResult()
{
    return std::make_shared<GetResult>();
}

void GetTask::printResultLog(const ResultPtr& result)
{
    if (m_longLogEnable) {
        qInfo() << QStringLiteral("Task finished, isSuccess: %1, errorMsg: %2, httpCode: %4, handle result elapsedTime: %5ms;").arg(result->isSuccess() ? "true" : "false").arg(result->errorMsg()).arg(result->m_httpCode).arg(m_elapsedTimer.elapsed())
                << "result: " << result->getJsonObject();
    } else {
        qInfo() << QStringLiteral("Task finished, isSuccess: %1, errorMsg: %2, httpCode: %4, handle result elapsedTime: %5ms").arg(result->isSuccess() ? "true" : "false").arg(result->errorMsg()).arg(result->m_httpCode).arg(m_elapsedTimer.elapsed());
    }
}
