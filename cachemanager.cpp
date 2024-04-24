#include "cachemanager.h"
#include <QDebug>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <QtConcurrent>
#include <chrono>
#include <fstream>
#include <thread>

using namespace Net;
std::mutex CacheManager::_mutex;
CacheManager& CacheManager::instance()
{
    static CacheManager myInstance;
    return myInstance;
}

QString CacheManager::getCacheDirectory(bool usingOneFolder, const QString& path)
{
    if (usingOneFolder) {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    static QString cacheDir;
    if (!cacheDir.isEmpty())
        return cacheDir;

    if (!path.isEmpty()) {
        cacheDir = path;
        std::thread th([=]() {
            auto start = std::chrono::steady_clock::now();
            QString lockFile = cacheDir + "/0^0.lock";
            static QLockFile fileLock(lockFile);
            fileLock.tryLock(5000);
            auto end = std::chrono::steady_clock::now();
            qInfo() << "switch account restart elapsed time in milliseconds: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms";
        });
        th.detach();
        return cacheDir;
    }

    std::unique_lock<decltype(_mutex)> lock(_mutex);
    bool bFind = false;
    cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);
    if (!dir.exists()) {
        if (!dir.mkpath(cacheDir)) {
            qInfo() << "mkpath faild:" << cacheDir;
        }
    }
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    for (QFileInfo& file : fileList) {
        QString footmarkFile = file.filePath() + "/0^0.footmark";
        if (!file.exists(footmarkFile)) {
            continue;
        }

        QString dFile = file.filePath() + "/0^0.delete";
        if (file.exists(dFile)) {
            continue;
        }

        QString lockFile = file.filePath() + "/0^0.lock";
        QLockFile fileLock(lockFile);
        if (fileLock.tryLock()) {
            fileLock.unlock();

            static QLockFile sFileLock(lockFile);
            if (sFileLock.tryLock()) {
                bFind = true;
                cacheDir = file.filePath();
                break;
            }
        }
    }

    if (!bFind) {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp
            = std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now());
        auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        cacheDir += QString("/%1").arg(tmp.count());
        if (!dir.mkpath(cacheDir)) {
            qInfo() << "mkpath faild:" << cacheDir;
        }

        QString footmarkFile = cacheDir + "/0^0.footmark";
#ifdef Q_OS_WIN32
        std::ofstream output(footmarkFile.toStdWString());
#else
        std::ofstream output(footmarkFile.toStdString());
#endif
        output.close();

        QString lockFile = cacheDir + "/0^0.lock";
        static QLockFile fileLock(lockFile);
        fileLock.tryLock();
    }
    qInfo() << "cacheDir :" << cacheDir;
    return cacheDir;
}

void CacheManager::setLockTag(quint64 uid)
{
    QString cacheDir = getCacheDirectory();
    QString footmarkFile = cacheDir + QString("/%1.footmark").arg(uid);
#ifdef Q_OS_WIN32
    std::ofstream output(footmarkFile.toStdWString());
#else
    std::ofstream output(footmarkFile.toStdString());
#endif
    output.close();
}

void CacheManager::setClearTag(quint64)
{
    auto markDelete = [](const QString& path) {
        QString dFile = path + "/0^0.delete";
#ifdef Q_OS_WIN32
        std::ofstream output(dFile.toStdWString());
#else
        std::ofstream output(dFile.toStdString());
#endif
        output.close();
    };
    markDelete(getCacheDirectory());

    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    qInfo() << "cacheDir :" << cacheDir;
    QDir dir(cacheDir);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    for (QFileInfo& file : fileList) {
        if (file.isDir()) {
            QString footmarkFile = file.filePath() + "/0^0.footmark";
            if (file.exists(footmarkFile)) {
                QString lockFile = file.filePath() + "/0^0.lock";
                QLockFile fileLock(lockFile);
                if (!fileLock.tryLock()) {
                    continue;
                }
                fileLock.unlock();
            }
            markDelete(file.filePath());
        }
    }
}

CacheManager::CacheManager()
    : QObject(nullptr)
{
    QtConcurrent::run(clearCache);
}

CacheManager::~CacheManager() { }

void CacheManager::clearCache()
{
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::unique_lock<decltype(_mutex)> lock(_mutex);
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    for (QFileInfo& file : fileList) {
        if (file.isDir()) {
            QString footmarkFile = file.filePath() + "/0^0.footmark";
            if (file.exists(footmarkFile)) {
                QString lockFile = file.filePath() + "/0^0.lock";
                QLockFile fileLock(lockFile);
                if (!fileLock.tryLock()) {
                    continue;
                }
                fileLock.unlock();

                QString dFile = file.filePath() + "/0^0.delete";
                if (file.exists(dFile)) {
                    QDir tmp(file.filePath());
                    tmp.setFilter(QDir::AllEntries | QDir::NoDotDot);
                    tmp.removeRecursively();
                }
            }
        }
    }
}
