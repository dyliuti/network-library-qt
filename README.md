- [网络库使用说明](#网络库使用说明)
  - [请求结果处理说明](#请求结果处理说明)
  - [请求调用示例](#请求调用示例)
    - [Get: 组装好url，获取对应Task就可以了。](#get-组装好url获取对应task就可以了)
    - [Post示例：以application/x-www-form-urlencoded为例](#post示例以applicationx-www-form-urlencoded为例)
    - [下载示例: 传保存路径下载到本地，不传保存路径下载到缓存](#下载示例-传保存路径下载到本地不传保存路径下载到缓存)
    - [上传示例：](#上传示例)
  - [子线程或线程池中执行请求说明](#子线程或线程池中执行请求说明)


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

## 请求结果处理说明

推荐使用task->run()回调方式获取结果。若没有或无需caller（即第一个参数this，父对象检测有效性）时，才使用futrue方式。
通过回调或future或连接信号得到请求结果result后，是否成功所有请求判断一样，isSucess代表网络正常+服务返回的数据正常
errorMsg()表示各请求对应的错误信息，因此isSuccess(),errorMsg是虚函数，各个请求对应的这两个方法需根据自已公司情况重写，重写后各请求使用、结果判断、处理都统一，如下：

```c++
task->run(this, [=](Net::ResultPtr result) {    // 通过回调
//auto future = task->run();                    // 通过future
//future.Then(this, [=](Net::ResultPtr result) {
    if (!result->isSuccess()) { // 通用请求结果处理
        // 请求错误
        qInfo() << result->errorMsg();
        return;
    } 
    // 请求成功
});
```

数据获取，可以通过Net::Result中的数据转换接口获取，如
getBytesData(): 获取请求返回的字节流数据
getJsonObject(): 获取请求返回的json对象数据，用于非规范的服务返回数据
errorMsg(): 获取具体的错误信息，每个请求类对应的结果类中可重写(根据自已公司返回的数据格式)，从而通过多态达到请求结果处理一致效果

## 请求调用示例
### Get: 组装好url，获取对应Task就可以了。

  ```c++
    QString url = "https://www.baidu.com";
    /*** 请求能力设置可选 ***/
    auto task = Net::Util::GetInstance().getGetTask(url); 
    task->run(this, [=](Net::ResultPtr result) { // 3.通用请求结果处理
        if (!result->isSuccess()) { // 通用请求结果处理
            // 请求错误
            qInfo() << result->errorMsg();
            return;
        } 
        // 请求成功
    });
  ```

### Post示例：以application/x-www-form-urlencoded为例

  ```c++
    QString url = "https://www.baidu.com";
    /*** 请求能力设置可选 ***/
    auto task = Net::Util::GetInstance().getPostByUrlEncodeTask(url); 
    task->run(this, [=](Net::ResultPtr result) { 
        if (!result->isSuccess()) { // 通用请求结果处理
            // 请求错误
            qInfo() << result->errorMsg();
            return;
        } 
        // 请求成功
    });
  ```

### 下载示例: 传保存路径下载到本地，不传保存路径下载到缓存

  ```c++
    QString url = "https://www.baidu.com";
    QString savePath = "";
    /*** 请求能力设置可选 ***/
    auto task = Net::Util::GetInstance().getDownloadTask(url, savePath); 
    task->run(this, [=](Net::ResultPtr result) { 
        if (!result->isSuccess()) { // 通用请求结果处理
            // 请求错误
            qInfo() << result->errorMsg();
            return;
        } 
        // 请求成功
    });
  ```

### 上传示例：
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
    task->run(this, [=](Net::ResultPtr result) {if (!result->isSuccess()) { // 通用请求结果处理
        if (!result->isSuccess()) { // 通用请求结果处理
            // 请求错误
            qInfo() << result->errorMsg();
            return;
        } 
        // 请求成功
    });
```

## 子线程或线程池中执行请求说明

Net::Util可以在线程中执行，使用run的回调可以保证结果是在调用者线程中处理。但future不一定能保证，得具体场景具体分析。

线程中执行的任务需要加上QEventLoop，进行事件接收。如下,QThread::create也可替换为QThreadpool::globalInstatnce().start(lambada), lambdada中内容一致

```C++
auto thread = QThread::create([=]() {
        QEventLoop eventLoop;
        auto task = Net::Util::instance().getDownloadTask(url, savePath);
        //    task->setDownloadLimit(1024 * 1024);
        //        connect(task.get(), &Net::DownloadTask::sigTaskOver, &eventLoop, &QEventLoop::quit);
        task->run(this, [=, &eventLoop](Net::ResultPtr result) {
            if (!result->isSuccess()) { // 通用请求结果处理
                // 请求错误
                qInfo() << result->errorMsg();
                return;
            }
            // 请求成功

            eventLoop.quit(); // 连sigTaskOver退出事件循环，或在回调中退出， 二选一，future类似
        });
        eventLoop.exec();
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
```

future 在then调用指定函数的时候，如果第一个参数传入继承自QObject类的指针，那么函数执行时候，所在的线程既是该指针所归属的线程，该调用方式如下

```C++
auto future = Net::Util::GetInstance().getPostByJsonTask(url, params)->run();
future.Then(this, [=](EeoNetworkResultPtr result) {}
```

其中this决定了函数的执行线程
如果第一个参数没有传入任何值，那么运行的线程是和调用m_promise.setValue(result)时，所在的线程决定的
涉及到多线程操作的时候，这一点还是需要注意的，需要清晰的知道自己每一行代码具体是执行在哪个线程之中，不然就可能出现和预期不一致的现象
