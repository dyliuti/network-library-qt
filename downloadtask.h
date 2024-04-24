#ifndef NETWORK_DOWNLOAD_TASK_H
#define NETWORK_DOWNLOAD_TASK_H
#include "gettask.h"
#include <QFile>
#include <QTimer>

namespace Net {
class NETWORK_EXPORT DownloadResult : public Result {
public:
    friend class DownloadTask;
    enum class SaveStatus {
        Success = 0,
        SavePathOpenError,
        SaveWriteError,
        SaveError
    };
    bool isSuccess() override;
    QString errorMsg(const QString& customErrorMsg) override;

protected:
    SaveStatus m_saveStatus = SaveStatus::Success;
};

class NETWORK_EXPORT DownloadTask : public GetTask {
    Q_OBJECT
public:
    DownloadTask(const QString& url); // 下载到缓存
    DownloadTask(const QString& url, const QString& savePath); // 下载到文件
    DownloadTask& setCalcSpeed(bool calcSpeed);
    DownloadTask& setDownloadLimit(qint64 bytesPerSecond);
    //    DownloadTask &setThreadPoolEnable(bool enable);

public:
signals:
    void sigDownloadProcess(qint64 bytesReceived, qint64 bytesTotal);
    void sigDownloadSpeed(qint64 bytesReceived, qint64 ms);

protected:
    QNetworkReply* execute() override;
    ResultPtr createResult() override;
    void notifyResult(const ResultPtr& result) override;
    void getBytesFromReply(const ResultPtr& result, QNetworkReply* reply) override;

protected slots:
    void onDownloadLimitProcess();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onReading();

private:
    bool isSaveToFile();
    void readAndSaveToFile(QNetworkReply* reply, QFile* file);
    bool openFile();

private:
    std::shared_ptr<DownloadResult> m_result;
    QString m_savePath;
    bool m_calcSpeed = false;
    //    bool m_threadPoolEnable = true;
    std::unique_ptr<QFile> m_file;
    qint64 m_maxBandwidth = 0; // 下载限速 每秒字节数 bytes/秒

    qint64 m_fileSize = 0;
    qint64 m_receviedBytesSize = 0;
    qint64 m_prevTime = 0;
    qint64 m_prevReceiveBytes = 0;
    QPointer<QTimer> m_calcProcessTimer;
};
}
#endif // NETWORK_DOWNLOAD_TASK_H
