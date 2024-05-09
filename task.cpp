#include "task.h"
#include "cachemanager.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QTimer>
#include <ctime>

using namespace Net;
/*** 网络请求结果 ***/
bool Result::networkSuccess()
{
    return m_qtNetworkError == QNetworkReply::NoError;
}

bool Result::isSuccess()
{
    return networkSuccess();
}

QString Result::networkErrorMsg(const QString& customErrorMsg)
{
    // customize error msg
    return "";
}

QString Result::errorMsg(const QString& customErrorMsg)
{
    return networkErrorMsg(customErrorMsg);
}

QString Result::errorMsg()
{
    return errorMsg("");
}

const QJsonObject& Result::getJsonObject()
{
    if (m_result.isEmpty()) {
        m_result = QJsonDocument::fromJson(m_byteArr).object();
    }

    return m_result;
}

/*** 网络请求基类 ***/
static std::once_flag s_onceFlag;
Task::Task(const QString& url)
    : m_url(url)
{
    std::call_once(s_onceFlag, [=]() {
        qRegisterMetaType<ResultPtr>("ResultPtr");
    });
    qInfo() << QStringLiteral("Task begin, url: %1").arg(m_url);
    m_request = QNetworkRequest(QUrl(m_url));
}

Task::~Task()
{
}

Task& Task::setRerequestCount(int rerequestCount)
{
    m_rerequestCount = rerequestCount;
    return *this;
}

Task& Task::setTimeout(int timeout)
{
    m_timeout = timeout;
    return *this;
}

Task& Task::setCacheEnable(bool enable)
{
    m_cacheEnable = enable;
    return *this;
}

Task& Task::setSignEnable(bool enable)
{
    m_signEnable = enable;
    return *this;
}

void Task::abort()
{
    if (m_networkReply && !m_networkReply->isFinished()) {
        m_networkReply->abort();
    }
}

void Task::retry()
{
    deleteNetworkReply();
    executeInner();
}

void Task::onRequestFinished()
{
    qInfo() << QStringLiteral("Task finished, async request elapsedTime: %1ms, url: %2").arg(m_elapsedTimer.elapsed()).arg(m_url);
    m_elapsedTimer.restart();

    const auto& result = parseReply(m_networkReply);
    // 重定向
    if (result->m_statusCode == Result::RequestStatus::Redirect) {
        const QUrl& newUrl = m_networkReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        m_request.setUrl(newUrl);
        retry();
        return;
    }

    // 失败重新请求
    if ((!result->isSuccess()) && m_rerequestCount > 0) {
        m_rerequestCount--;
        retry();
        return;
    }

    deleteNetworkReply();
    printResultLog(result);
    notifyResult(result);
}

void Task::notifyResult(const ResultPtr& result)
{
    if (m_caller && m_completeCallback) {
        m_completeCallback(result);
    } else {
        m_promise.setValue(result);
    }

    emit sigTaskOver(result);
}

void Task::onCopeSslErrors(const QList<QSslError>& errors)
{
    if (m_networkReply == nullptr) {
        return;
    }
    static QSet<QSslError::SslError> whiteListErrors;
    whiteListErrors << QSslError::UnableToGetLocalIssuerCertificate
                    << QSslError::UnableToVerifyFirstCertificate
                    << QSslError::CertificateUntrusted
                    << QSslError::HostNameMismatch;

    QSet<QSslError::SslError> occuredErrors;
    foreach (const auto& e, errors) {
        occuredErrors << e.error();
    }

    QSet<QSslError::SslError> unexpectedSslErrors = occuredErrors - whiteListErrors;
    if (unexpectedSslErrors.isEmpty()) {
        m_networkReply->ignoreSslErrors();
    } else {
        emit sigSslErrors(errors);
    }
}

std::once_flag s_netManagerOnceFlag;
static QNetworkAccessManager* s_netManager = nullptr;
QNetworkAccessManager* Task::getNetworkAccessManager()
{
    std::call_once(s_netManagerOnceFlag, [=]() {
        s_netManager = new ::QNetworkAccessManager();
        auto diskCache = new QNetworkDiskCache(s_netManager);
        diskCache->setMaximumCacheSize(1024 * 1024 * 512); // 512M
        diskCache->setCacheDirectory(CacheManager::instance().getCacheDirectory(false));
        s_netManager->setCache(diskCache);
    });

    return s_netManager;
}

Async::Future<ResultPtr> Task::run()
{
    runInner();
    return m_promise.getFuture();
}

void Task::run(QPointer<QObject> caller, std::function<void(ResultPtr)> completeCallback)
{
    m_caller = caller;
    m_completeCallback = completeCallback;
    runInner();
}

void Task::runInner()
{
    setRequestContentType();
    setRequestCache();
    if (m_signEnable) {
        setRequestCustomHeader(); // 自定义Header,服务端校验等用
    }
    // setRequestSslConfig();
    setAbortWhenTimeout();
    m_elapsedTimer.start();
    executeInner();
}

void Task::executeInner()
{
    auto networkReply = execute();
    if (networkReply == nullptr) {
        return;
    }

    m_networkReply = networkReply;
    connect(m_networkReply, &QNetworkReply::finished, this, &Task::onRequestFinished);
    connect(m_networkReply, &QNetworkReply::sslErrors, this, &Task::onCopeSslErrors);
}

ResultPtr Task::createResult()
{
    return std::make_unique<Result>();
}

void Task::printResultLog(const ResultPtr& result)
{
    qInfo() << QStringLiteral("Task finished, isSuccess: %1, errorMsg: %2, httpCode: %3, handle result elapsedTime: %4ms").arg(result->isSuccess() ? "true" : "false").arg(result->errorMsg()).arg(result->m_httpCode).arg(m_elapsedTimer.elapsed());
}

QJsonObject Task::convetJsonValueToString(const QJsonObject& obj)
{
    QJsonObject res;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        QString value;
        if (it.value().isObject()) {
            value = QString(QJsonDocument(it.value().toObject()).toJson());
        } else if (it.value().isArray()) {
            value = QString(QJsonDocument(it.value().toArray()).toJson());
        } else if (it.value().isString()) {
            value = it.value().toString();
        } else {
            value = it.value().toVariant().toString();
        }
        res.insert(it.key(), value);
    }

    return res;
}

void Task::setRequestContentType()
{
    const auto& contentType = getContentType();
    if (!contentType.isEmpty()) {
        m_request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
}

void Task::setRequestCache()
{
    m_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
        m_cacheEnable ? QNetworkRequest::PreferNetwork
                      : QNetworkRequest::AlwaysNetwork);
    m_request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, m_cacheEnable);
    if (m_cacheEnable == false) {
        // for tencent cdn, "Cache-Control: no-cache" has no effect, we have to use private header to get fresh file
        m_request.setRawHeader("EEO-Cache-Control", "no-cache");
    }
}

void Task::setRequestCustomHeader()
{
}

ResultPtr Task::parseReply(QNetworkReply* reply)
{
    auto result = createResult();
    result->m_httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).value<int>();
    result->m_qtNetworkError = reply->error();
    result->m_qtErrorString = reply->errorString();
    if (result->m_httpCode >= 400 && result->m_httpCode < 500) {
        result->m_statusCode = Result::RequestStatus::ClientError;
    } else if (result->m_httpCode >= 500 && result->m_httpCode <= 600) {
        result->m_statusCode = Result::RequestStatus::ServerError;
    } else if (result->m_httpCode >= 300 && result->m_httpCode < 400) {
        result->m_statusCode = Result::RequestStatus::Redirect;
    } else if (result->m_httpCode < 200) {
        result->m_statusCode = Result::RequestStatus::NetworkError;
    } else {
        result->m_statusCode = Result::RequestStatus::Success;
    }

    getBytesFromReply(result, reply);

    result->m_taskId = m_taskId;

    return result;
}

void Task::getBytesFromReply(const ResultPtr& result, QNetworkReply* reply)
{
    result->m_byteArr = reply->readAll();
}

void Task::deleteNetworkReply()
{
    if (m_networkReply) {
        m_networkReply->deleteLater();
        m_networkReply = nullptr;
    }
}

void Task::setRequestSslConfig()
{
    QSslConfiguration config;
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    m_request.setSslConfiguration(config);
}

void Task::setAbortWhenTimeout()
{
    if (m_timeout > 0) {
        QTimer::singleShot(m_timeout, this, [=]() {
            qInfo() << QStringLiteral("Task url: %1, abort because exceded the setted timeout of %2ms").arg(m_url).arg(m_timeout);
            abort();
        });
    }
}

Runnable::Runnable(std::function<void()> func)
    : m_func(func)
{
}

void Runnable::run()
{
    if (m_func) {
        m_func();
    }
}
