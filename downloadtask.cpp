#include "downloadtask.h"
#include <QDir>
#include <QFileInfo>
#include <QThreadPool>

using namespace Net;
bool DownloadResult::isSuccess()
{
    return networkSuccess() && m_saveStatus == SaveStatus::Success;
}

QString DownloadResult::errorMsg(const QString& customErrorMsg)
{
    Q_UNUSED(customErrorMsg);
    if (m_saveStatus != SaveStatus::Success) {
        return "download failed";
    }

    return "success";
}

DownloadTask::DownloadTask(const QString& url)
    : GetTask(url)
{
    m_timeout = 0;
    m_signEnable = false;
}

DownloadTask::DownloadTask(const QString& url, const QString& savePath)
    : GetTask(url)
    , m_savePath(savePath)
{
    m_timeout = 0;
    m_signEnable = false;
}

DownloadTask& DownloadTask::setCalcSpeed(bool calcSpeed)
{
    m_calcSpeed = calcSpeed;
    return *this;
}

DownloadTask& DownloadTask::setDownloadLimit(qint64 bytesPerSecond)
{
    m_maxBandwidth = bytesPerSecond;
    return *this;
}

// DownloadTask &DownloadTask::setThreadPoolEnable(bool enable)
//{
//     m_threadPoolEnable = enable;
//     return *this;
// }

/*** download请求 ***/
QNetworkReply* DownloadTask::execute()
{
    m_result.reset();
    createResult();
    if (!m_savePath.isEmpty()) {
        if (false == openFile()) {
            notifyResult(m_result);
        }
    }

    auto networkReply = getNetworkAccessManager()->get(m_request);

    m_prevTime = QDateTime::currentMSecsSinceEpoch();
    if (m_maxBandwidth > 0) { // 限速
        m_receviedBytesSize = 0;
        m_prevReceiveBytes = 0; // 重置限速计算初始变量
        connect(networkReply, &QNetworkReply::metaDataChanged, this, [=] {
            m_fileSize = networkReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        });

        networkReply->setReadBufferSize(m_maxBandwidth * 2);
        if (m_calcProcessTimer == nullptr) {
            m_calcProcessTimer = new QTimer(this);
        }
        connect(m_calcProcessTimer, &QTimer::timeout, this, &DownloadTask::onDownloadLimitProcess);
        m_calcProcessTimer->start(100); // 定时器每100毫秒触发一次
    } else {
        connect(networkReply, &QNetworkReply::readyRead, this, &DownloadTask::onReading);
        connect(networkReply, &QNetworkReply::downloadProgress, this, &DownloadTask::onDownloadProgress);
    }

    return networkReply;
}

ResultPtr DownloadTask::createResult()
{
    if (m_result == nullptr) {
        m_result = std::make_shared<DownloadResult>();
    }
    return m_result;
}

void DownloadTask::getBytesFromReply(const ResultPtr& result, QNetworkReply* reply)
{
    Q_UNUSED(result);
    if (isSaveToFile()) {
        readAndSaveToFile(m_networkReply, m_file.get());
        m_file->close();
        m_file.reset();
    } else {
        m_result->m_byteArr.push_back(reply->readAll());
    }
}

bool DownloadTask::openFile()
{
    if (m_file) {
        m_file->close();
        m_file.reset();
    }

    QFileInfo info(m_savePath);
    if (info.exists()) {
        QFile::remove(m_savePath);
    }
    QString dirPath = info.dir().absolutePath();
    if (!QFileInfo::exists(dirPath)) {
        QDir().mkpath(dirPath);
    }

    m_file = std::make_unique<QFile>(m_savePath);
    if (m_file->open(QIODevice::WriteOnly)) {
        if (!m_file->isWritable()) {
            m_result->m_saveStatus = DownloadResult::SaveStatus::SaveWriteError;
            return false;
        }

        m_result->m_saveStatus = DownloadResult::SaveStatus::Success;
        return true;
    }

    m_result->m_saveStatus = DownloadResult::SaveStatus::SavePathOpenError;
    return false;
}

// 限速读取与进度
void DownloadTask::onDownloadLimitProcess()
{
    if (m_networkReply == nullptr) {
        return;
    }
    qint64 curTime = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsedTime = curTime - m_prevTime;
    qint64 expectedBytes = m_maxBandwidth * elapsedTime / 1000;
    qint64 bytesToRead = qMin(m_networkReply->bytesAvailable(), expectedBytes);
    const auto& data = m_networkReply->read(bytesToRead);
    if (isSaveToFile()) {
        m_file->write(data);
    } else {
        m_result->m_byteArr.push_back(data);
    }
    m_receviedBytesSize += bytesToRead;
    emit sigDownloadProcess(m_receviedBytesSize, m_fileSize);
    if (m_calcSpeed) {
        emit sigDownloadSpeed(m_receviedBytesSize - m_prevReceiveBytes, elapsedTime);
    }
    m_prevTime = curTime;
    m_prevReceiveBytes = m_receviedBytesSize;
}

void DownloadTask::notifyResult(const ResultPtr& result)
{
    if (m_calcProcessTimer) { // abort也会进finished
        m_calcProcessTimer->stop();
    }

    if (m_calcSpeed) {
        if (m_savePath.isEmpty()) {
            emit sigDownloadSpeed(result->m_byteArr.size() - m_prevReceiveBytes, QDateTime::currentMSecsSinceEpoch() - m_prevTime);
        } else {
            emit sigDownloadSpeed(m_fileSize - m_prevReceiveBytes, QDateTime::currentMSecsSinceEpoch() - m_prevTime);
        }
    }

    if (m_savePath.isEmpty()) {
        emit sigDownloadProcess(result->m_byteArr.size(), result->m_byteArr.size());
    } else {
        emit sigDownloadProcess(m_fileSize, m_fileSize);
    }

    GetTask::notifyResult(result);
}

// 不限速进度
void DownloadTask::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_calcSpeed) {
        auto curTime = QDateTime::currentMSecsSinceEpoch();
        emit sigDownloadSpeed(bytesReceived - m_prevReceiveBytes, curTime - m_prevTime);
        m_prevTime = curTime;
        m_prevReceiveBytes = bytesReceived;
    }

    emit sigDownloadProcess(bytesReceived, bytesTotal);
}

// 不限速读取
void DownloadTask::onReading()
{
    if (isSaveToFile()) {
        readAndSaveToFile(m_networkReply, m_file.get());
    } else {
        m_result->m_byteArr.push_back(m_networkReply->readAll());
    }
}

bool DownloadTask::isSaveToFile()
{
    return !m_savePath.isEmpty() && m_file;
}

void DownloadTask::readAndSaveToFile(QNetworkReply* reply, QFile* file)
{
    if (reply == nullptr || file == nullptr) {
        return;
    }

    while (!reply->atEnd()) {
        QByteArray bytes;
        static int maxReadSizeOnce = 1024 * 1024 * 10; // 10M
        if (reply->bytesAvailable() > maxReadSizeOnce) {
            bytes = reply->read(maxReadSizeOnce);
        } else {
            bytes = reply->readAll();
        }

        file->write(bytes);
    }
}
