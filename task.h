#ifndef NETWORK_TASK_H
#define NETWORK_TASK_H
#include "async/future.h"
#include "network_global.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QRunnable>

namespace Net {
class Result;
}
Q_DECLARE_METATYPE(Net::Result)

class QNetworkAccessManager;
namespace Net {
// 调用举例文档：InstructionForUse.h
class NETWORK_EXPORT Result {
public:
    friend class Task;
    friend class GetTask;
    friend class PostTask;
    friend class DownloadTask;
    friend class Util;
    enum class RequestStatus {
        Success = 0,
        ClientError,
        ServerError,
        Redirect,
        NetworkError,
        UnknowError
    };
    // 是否超时后客户端主动调用abort断开请求
    bool abortByTimeOut() { return m_qtNetworkError == QNetworkReply::OperationCanceledError; }
    bool networkSuccess(); // 只是单纯网络没错误，不代表请求结果没问题
    QString networkErrorMsg(const QString& customErrorMsg); // 网络错误信息; 使用标准http错误码提示
    virtual bool isSuccess(); // 请求是否正确：网络没问题 + 结果也正确
    /*** 对应之前的 convertGeneralError(下载) 与 networkErrorToString(post请求) ***/
    virtual QString errorMsg(const QString& customErrorMsg); // 请求错误信息：networkErrorMsg(网络错误) + 自定义错误信息; 使用标准http错误码提示
    virtual QString errorMsg(); // 请求错误信息：networkErrorMsg(网络错误) + webErrorMsg(前端错误信息)或保存错误; 使用标准http错误码提示
    int qtNetworkError() { return m_qtNetworkError; } // 对应之前 networkError
    QString qtErrorString() { return m_qtErrorString; } // 对应之前 errorString

    QByteArray getBytesData() { return m_byteArr; } // 获得请求返回的原始数据
    const QJsonObject& getJsonObject(); // 将请求返回的原始数据转换为JsonObject

protected:
    const QAtomicInteger<qint64>& getTaskId() { return m_taskId; }

protected:
    RequestStatus m_statusCode = RequestStatus::UnknowError; // 原始状态码转换为枚举类型
    int m_httpCode = -1; // 请求返回的http状态码 0-500
    QNetworkReply::NetworkError m_qtNetworkError; // qt的请求错误码 对应responsePod中的error
    QString m_qtErrorString; // qt的错误信息
    QByteArray m_byteArr; // 请求返回的数据
    QJsonObject m_result; // 缓存加速用
    QAtomicInteger<qint64> m_taskId = 0;
};

/*** 网络请求基类 ***/
typedef std::shared_ptr<Result> ResultPtr;
class NETWORK_EXPORT Task : public QObject {
    Q_OBJECT
public:
    friend class Util;
    Task(const QString& url);
    virtual ~Task();
    Task& setRerequestCount(int rerequestCount);
    Task& setTimeout(int timeout); // 单位: milliseconds
    Task& setCacheEnable(bool enable);
    Task& setSignEnable(bool enable);
    void abort();
    void retry();
    Async::Future<ResultPtr> run();
    void run(QPointer<QObject> caller, std::function<void(ResultPtr)> completeCallback);

public:
signals:
    void sigSslErrors(const QList<QSslError>& errors);

protected:
signals:
    void sigTaskOver(ResultPtr result);

protected slots:
    void onRequestFinished();
    void onCopeSslErrors(const QList<QSslError>& errors);

protected:
    static QNetworkAccessManager* getNetworkAccessManager();
    virtual QNetworkReply* execute() = 0;
    virtual QString getContentType() = 0;
    virtual ResultPtr createResult();
    virtual void notifyResult(const ResultPtr& result);
    virtual void printResultLog(const ResultPtr& result);
    QJsonObject convetJsonValueToString(const QJsonObject& obj);

private:
    void setRequestContentType();
    void setRequestCache();
    void setRequestCustomHeader();
    void setRequestSslConfig();
    void setAbortWhenTimeout();
    ResultPtr parseReply(QNetworkReply* reply);
    virtual void getBytesFromReply(const ResultPtr& result, QNetworkReply* reply);
    void setTaskId(const QAtomicInteger<qint64>& id) { m_taskId = id; }
    const QAtomicInteger<qint64>& getTaskId() { return m_taskId; }
    void deleteNetworkReply();
    void runInner();
    void executeInner();

protected:
    QString m_url;
    QNetworkReply* m_networkReply = nullptr;
    QNetworkRequest m_request;
    Async::Promise<ResultPtr> m_promise;
    QPointer<QObject> m_caller;
    std::function<void(ResultPtr)> m_completeCallback;
    QElapsedTimer m_elapsedTimer;

    int m_rerequestCount = 0; // 请求失败重试次数
    int m_timeout = 15 * 1000; // 客户端请求超时主动断开时间 单位: milliseconds
    bool m_cacheEnable = false;
    bool m_signEnable = true;
    QAtomicInteger<qint64> m_taskId = 0;
};

class Runnable : public QRunnable {
public:
    Runnable(std::function<void()> func);
    void run() override;

private:
    std::function<void()> m_func;
};
}
#endif // NETWORK_TASK_H
