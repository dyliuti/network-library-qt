QT -= gui
QT += core network svg concurrent
TEMPLATE = lib
DEFINES += NETWORK_LIBRARY

TARGET = net
CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    InstructionForUse.h \
    async/async.h \
    async/future.h \
    async/helper.h \
    async/scheduler.h \
    async/sharedpromise.h \
    async/threadPool.h \
    async/try.h \
    cachemanager.h \
    downloadtask.h \
    gettask.h \
    network_global.h \
    posttask.h \
    task.h \
    uploadtask.h \
    util.h

SOURCES += \
    async/threadPool.cpp \
    cachemanager.cpp \
    downloadtask.cpp \
    gettask.cpp \
    posttask.cpp \
    task.cpp \
    uploadtask.cpp \
    util.cpp
