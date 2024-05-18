- [网络库使用说明](#网络库使用说明)
  - [请求能力说明](#请求能力说明)
    - [通用能力(在基类 Net::Task 中)](#通用能力在基类-nettask-中)
    - [下载类 Net::DownloadTask 额外包含的能力](#下载类-netdownloadtask-额外包含的能力)
    - [上传类 Net::UploadTask 额外包含的能力：](#上传类-netuploadtask-额外包含的能力)
  - [请求结果处理说明](#请求结果处理说明)
  - [请求调用示例](#请求调用示例)
    - [Get: 组装好 url，获取对应 Task 就可以了。](#get-组装好-url获取对应-task-就可以了)
    - [Post 示例：以 application/x-www-form-urlencoded 为例](#post-示例以-applicationx-www-form-urlencoded-为例)
    - [下载示例: 传保存路径下载到本地，不传保存路径下载到缓存](#下载示例-传保存路径下载到本地不传保存路径下载到缓存)
    - [上传示例：](#上传示例)
  - [子线程或线程池中执行请求说明](#子线程或线程池中执行请求说明)
- [网络库优点列举](#网络库优点列举)

# 网络库使用说明

支持版本：Windows、Mac、linux 平台。Qt5、Qt6 所有版本都可以，

Net::Util 中帮用户管理了请求类的生命周期，提供给用户所有请求类的不同参数重载接口，用户按需进行获取。

Net::Result 是请求结果基类。所有请求结果接口行为一致，是否成功用 isSuccess(), 失败获取错误信息用 errorMsg()。

Net::是基于多态编写的网络库。Net::Task 是请求基类，集合了请求的通用功能。用户请求时只需要从 Net::Util 获取对应的请求类，1.构造 url、参数，2.设置需要的请求功能(可选)；3.通过 run 中传入回调或 future 对结果进行处理即可。

## 请求能力说明

### 通用能力(在基类 Net::Task 中)

1. 是否设置缓存 setCacheEnable。默认 false。设置了缓存下次请求会很快（如下载就不需要再从网络重新下载了）。
2. 超时重传次数 setRerequestCount。默认不重传
3. 超时时间 setTimeout。默认为 0，不主动断开。超时后主动结束请求
4. 断开请求 abort。
5. 重新请求 retry。

### 下载类 Net::DownloadTask 额外包含的能力

1. 设置限速 setDownloadLimit。默认 false。
2. 是否开启下载速度计算 setCalcSpeed。默认 false。开启后可连接 sigDownloadSpeed 进行速度显示
3. 是否开启线程池执行任务 setThreadPoolEnable（已禁掉该方法）

### 上传类 Net::UploadTask 额外包含的能力：

1. 是否开启线程池执行任务 setThreadPoolEnable。默认 true 开启。如果调用上传类时就已经在线程中，可以将其设置为 false。

## 请求结果处理说明

推荐使用 task->run()回调方式获取结果。若没有或无需 caller（即第一个参数 this，父对象检测有效性）时，才使用 futrue 方式。
通过回调或 future 或连接信号得到请求结果 result 后，是否成功所有请求判断一样，isSucess 代表网络正常+服务返回的数据正常
errorMsg()表示各请求对应的错误信息，因此 isSuccess(),errorMsg 是虚函数，各个请求对应的这两个方法需根据自已公司情况重写，重写后各请求使用、结果判断、处理都统一，如下：

```c++
task->run(this, [=](Net::ResultPtr result) {    // 通过回调
//auto future = task->run();                    // 通过future
//future.then(this, [=](Net::ResultPtr result) {
    if (!result->isSuccess()) { // 通用请求结果处理
        // 请求错误
        qInfo() << result->errorMsg();
        return;
    }
    // 请求成功
});
```

数据获取，可以通过 Net::Result 中的数据转换接口获取，如
getBytesData(): 获取请求返回的字节流数据
getJsonObject(): 获取请求返回的 json 对象数据，用于非规范的服务返回数据
errorMsg(): 获取具体的错误信息，每个请求类对应的结果类中可重写(根据自已公司返回的数据格式)，从而通过多态达到请求结果处理一致效果

## 请求调用示例

包含头文件 #include "network/util.h"

### Get: 组装好 url，获取对应 Task 就可以了。

```c++
  QString url = "https://www.baidu.com";
  /*** 请求能力设置可选 ***/
  auto task = Net::Util::instance().getGetTask(url);
  task->run(this, [=](Net::ResultPtr result) {
      if (!result->isSuccess()) { // 通用请求结果处理
          // 请求错误
          qInfo() << result->errorMsg();
          return;
      }
      // 请求成功
  });
```

### Post 示例：以 application/x-www-form-urlencoded 为例

```c++
  QString url = "https://www.baidu.com";
  /*** 请求能力设置可选 ***/
  auto task = Net::Util::instance().getPostByUrlEncodeTask(url);
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
  auto task = Net::Util::instance().getDownloadTask(url, savePath);
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
  task->run(this, [=](Net::ResultPtr result) {
      if (!result->isSuccess()) { // 通用请求结果处理
          // 请求错误
          qInfo() << result->errorMsg();
          return;
      }
      // 请求成功
  });
```

## 子线程或线程池中执行请求说明

Net::Util 可以在线程中执行，使用 run 的回调可以保证结果是在调用者线程中处理。但 future 不一定能保证，得具体场景具体分析。

线程中执行的任务需要加上 QEventLoop，进行事件接收。如下,QThread::create 也可替换为 QThreadpool::globalInstatnce().start(lambada), lambdada 中内容一致

```C++
auto thread = QThread::create([=]() {
        QEventLoop eventLoop;
        auto task = Net::Util::instance().getDownloadTask(url, savePath);
        //    task->setDownloadLimit(1024 * 1024);
        //    connect(task.get(), &Net::DownloadTask::sigTaskOver, &eventLoop, &QEventLoop::quit);
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

future 在 then 调用指定函数的时候，如果第一个参数传入继承自 QObject 类的指针，那么函数执行时候，所在的线程既是该指针所归属的线程，该调用方式如下

```C++
auto future = Net::Util::instance().getPostByJsonTask(url, params)->run();
future.then(this, [=](EeoNetworkResultPtr result) {}
```

其中 this 决定了函数的执行线程
如果第一个参数没有传入任何值，那么运行的线程是和调用 m_promise.setValue(result)时，所在的线程决定的
涉及到多线程操作的时候，这一点还是需要注意的，需要清晰的知道自己每一行代码具体是执行在哪个线程之中，不然就可能出现和预期不一致的现象

# 网络库优点列举

1.自动的生命周期管理，请求结束处理完后自动销毁请求类。Net::Util 返回的请求类当作成员变量也可延长请求类生命周期，此时跟随该请求所在的类。 2.方便且统一的网络请求使用、请求能力设置、结果处理。不管什么请求类型，能力设置、结果设置行为一致。不用再针对某种请求想着怎么判断是否成功，怎么设置请求功能(简化了)等。 3.易于维护、扩展的网络库代码。简化代码、清晰逻辑、便于阅读。抽象各个请求通用部分，不用再写重复代码、维护多份重复功能。 4.网络库与调用方完全分隔，网络库将作为一个独立的库，不依赖同级、上级的模块。不包含业务代码，上层只负责调用。
5.QNetworkAccessManager 全局统一只使用一个。
