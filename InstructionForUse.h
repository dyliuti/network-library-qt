#ifndef NETWORK_INSTRUCTION_FOR_USE_H
#define NETWORK_INSTRUCTION_FOR_USE_H

/***
 * 推荐优先使用回调方式获取结果(调用更加简单)。若没有或无需caller（父对象检测有效性）时，才使用futrue
 * 回调方式使用示例如下，可拷贝。getPostByUrlEncodeTask可更改其它请求Task，对应参数也更改下
 ***/

// #include "network/util.h"
// 非上传使用模板如下，可拷贝。get、post、download通用，类名或参数替换下就行
/***
    auto url = "domain/router";
    auto param = QJsonObject {
        {"key", "value"}
    }
    auto task = Net::Util::instance().getPostByUrlEncodeTask(url, param);
    task->run(this, [=](Net::ResultPtr result) {
        if (!result->isSuccess()) {
            // 请求错误处理
            return;
        }
        // 请求成功处理
    });
***/

// 上传使用模板如下，可拷贝。上传单个资源时：
/***
    auto url = "domain/router";
    auto textParam = QJsonObject {
        {"key", "value"}
    };
    QString filePath = ""; // 待上传的文件
    auto fileParam = std::make_shared<Net::UploadFilePathParam>("imageData", QFileInfo(filePath).fileName(), "application/octet-stream")->setResource(filePath);
    auto task = Net::Util::instance().getUploadTask(url, textParam, fileParam);
    connect(task.get(), &Net::UploadTask::sigUploadProgress, this, [=](qint64 bytesSent, qint64 bytesTotal){}); // 处理进度
    task->run(this, [=](Net::ResultPtr result) {
        if (!result->isSuccess()) {
            // 请求错误处理
            return;
        }
        // 请求成功处理
    });
***/

// 上传多个资源时：
/***
    auto url = "domain/router";
    auto textParam = QJsonObject {
        {"key", "value"}
    };
    QString filePath = ""; // 待上传的文件
    QPixmap pixmap = QPixmap(); // 待上传的图片
    QByteArray byteArray; // 待上传的数据（已转换到raw bytes）
    auto fileParam = std::make_shared<Net::UploadFilePathParam>("imageData", QFileInfo(filePath).fileName(), "application/octet-stream")->setResource(filePath);
    auto pixParam = std::make_shared<Net::UploadPixmapParam>("name", "filename", "image/jpeg")->setResource(std::move(pixmap));
    auto byteParam = std::make_shared<Net::UploadByteArrayParam>("name", "filename", "multipart/form-data")->setResource(byteArray);
    std::vector<Net::UploadResourceParamPtr> resourceVec{fileParam, pixParam, byteParam};
    auto task = Net::Util::instance().getUploadTask(url, textParam, resourceVec);
    connect(task.get(), &Net::UploadTask::sigUploadProgress, this, [=](qint64 bytesSent, qint64 bytesTotal){}); // 处理进度
    task->run(this, [=](Net::ResultPtr result) {
        if (!result->isSuccess()) {
            // 请求错误处理
            return;
        }
        // 请求成功处理
    });
***/

// 上传其他资源（QImage或自定义资源等）扩展说明
/***
 * 1.继承于UploadResourceParam<T>
 * 2.实现接口isEmpty() 与 getByteArray()。然后按照上述上传使用例子创建扩展类即可。
 * ***/
#endif // NETWORK_INSTRUCTION_FOR_USE_H
