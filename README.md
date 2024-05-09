# 网络库使用说明

Net::Util中帮用户管理了请求类的生命周期，提供给用户所有请求类的不同参数重载接口，用户按需进行获取。

Net::Result是请求结果基类。所有请求结果接口行为一致，是否成功用isSuccess(), 失败获取错误信息用errorMsg()。

Net::是基于多态编写的网络库。Net::Task是请求基类，集合了请求的通用功能。用户请求时只需要从Net::Util获取对应的请求类，1.构造url、参数，2.设置需要的请求功能(可选)；3.通过run中传入回调或Future或连接sigTaskOver信号对结果进行处理即可。

通用能力如下(在基类Net::Task中)：

1. 是否设置缓存 setCacheEnable。默认false。即之前的白名单，设置了缓存下次请求打开文件很快。
2. 超时重传次数 setRerequestCount。默认不重传
3. 超时时间 setTimeout。默认为0，不主动断开。超时后主动结束请求
4. 断开请求 abort。
5. 重新请求 retry。

下载类Net::DownloadTask额外包含的能力：

1. 设置限速 setDownloadLimit。默认false。
2. 是否开启下载速度计算 setCalcSpeed。默认false。开启后可连接sigDownloadSpeed进行速度显示
3. 是否开启线程池执行任务 setThreadPoolEnable。默认true开启。如果调用下载类时就已经在线程中，可以将其设置为false。

上传类Net::UploadTask额外包含的能力：

1. 是否开启线程池执行任务 setThreadPoolEnable。默认true开启。如果调用上传类时就已经在线程中，可以将其设置为false。

#### 请求结果处理说明

通过回调或future或连接信号得到请求结果result后，是否成功所有请求判断一样，isSucess代表网络正常+前端返回的字段正常 如下

```c++
task->run(this, [=](Net::ResultPtr result) {    // 通过回调
//auto future = task->run();                    // 通过future
//future.Then(this, [=](Net::ResultPtr result) {
    if (result->isSuccess()) {
    	// 请求成功
    } else {
        // 请求错误
    }
});
```

数据获取，可以通过Net::Result中的数据转换接口获取，如

getBytesData(): 获取请求返回的字节流数据

getJsonObject(): 获取请求返回的json对象数据，用于非规范的服务返回数据


#### 上传示例。
  ```c++
    QString url = "www.baidu.com";
    QJsonObject params;
    std::vector<UploadResourceParamPtr> resourceParams;
    QPixmap pixmap = QPixmap();
    QByteArray fileBytes = QByteArray();
    resourceParams.push_back(std::make_shared<UploadPixmapParam>(std::move(pixmap), "pix", "jpg", "image/jpeg"));
    resourceParams.push_back(std::make_shared<UploadPixmapParam>(std::move(fileBytes), "file", "txt", "multipart/form-data"));
    resourceParams.push_back(std::make_shared<UploadPixmapParam>("D:/b.txt", "bb", "bTxt", "multipart/form-data"));
    // 1.请求url与参数构造
    auto task = Net::Util::instance().getUploadTask(url, params, resourceParams);
    // 2.请求能力设置(可选)
    connect(task.get(), &Net::UploadTask::sigUploadProgress, this, [=](qint64 bytesReceived, qint64 bytesTotal) {
        // 上传进度业务代码
    });
    task->run(this, [=](Net::ResultPtr result) {}); // 3.通用结果处理
```

#### Get: 组装好url，获取对应Task就可以了。

  ```c++
  QString url = "https://www.baidu.com";
  	/*** 请求能力设置可选 ***/
      auto future = Net::Util::GetInstance().getGetTask(url)->run(); 
      future.Then(this, [=](Net::ResultPtr result) {}// 3.通用的结果处理
  ```
