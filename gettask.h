#ifndef NETWORK_GET_TASK_H
#define NETWORK_GET_TASK_H
#include "task.h"

namespace Net {
/***  Post请求结果 ***/
class NETWORK_EXPORT GetResult : public Result {
public:
    bool isSuccess() override;
    QString errorMsg(const QString& customErrorMsg) override;
};

class NETWORK_EXPORT GetTask : public Task {
    Q_OBJECT
public:
    GetTask(const QString& url);
    GetTask(const QString& url, const QJsonObject& obj);
    GetTask(const QString& url, const QVariantMap& vm);
    GetTask& setLongLogEnable(bool enable);

protected:
    QNetworkReply* execute() override;
    QString getContentType() override;
    ResultPtr createResult() override;
    void printResultLog(const ResultPtr& result) override;

private:
    QJsonObject m_params;
    bool m_longLogEnable = true;
};
}
#endif // NETWORK_GET_TASK_H
