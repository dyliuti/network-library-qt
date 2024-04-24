#ifndef ASYNC_H
#define ASYNC_H

#include "threadPool.h"

namespace Async {

template<typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
auto async(F &&f, Args &&...args) -> Future<typename std::result_of<F(Args...)>::type>
{
    return ThreadPool::globalInstance()->execute(f, args...);
}

template<typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
auto async(F &&f, Args &&...args) -> Future<void>
{
    return ThreadPool::globalInstance()->execute(f, args...);
}

// 避免在定时器所在线程wait
template<typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
auto async(std::chrono::milliseconds duration, F &&f, Args &&...args) -> Future<typename std::result_of<F(Args...)>::type>
{
    using resultType = typename std::result_of<F(Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    QTimer::singleShot(
        duration, [pm = std::move(promise), t = std::move(func)]() mutable {
            auto infuture = ThreadPool::globalInstance()->execute(t);
            infuture.then([pm = std::move(pm)](auto &&result) mutable {
                try {
                    pm.setValue(std::forward<decltype(result)>(result));
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            });
        });
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
auto async(std::chrono::milliseconds duration, F &&f, Args &&...args) -> Future<void>
{
    using resultType = typename std::result_of<F(Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    QTimer::singleShot(
        duration, [pm = std::move(promise), t = std::move(func)]() mutable {
            auto infuture = ThreadPool::globalInstance()->execute(t);
            infuture.then([pm = std::move(pm)]() mutable {
                try {
                    pm.setValue();
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            });
        });
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
auto async(QPointer<QObject> context, F &&f, Args &&...args) -> Future<typename std::result_of<F(Args...)>::type>
{
    using resultType = typename std::result_of<F(Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (context) {
        QObject obj;
        QObject::connect(
            &obj, &QObject::destroyed, context, [pm = std::move(promise), t = std::move(func)]() mutable {
                try {
                    pm.setValue(Try<resultType>(t()));
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            },
            Qt::QueuedConnection);
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
auto async(QPointer<QObject> context, std::chrono::milliseconds duration, F &&f, Args &&...args) -> Future<typename std::result_of<F(Args...)>::type>
{
    using resultType = typename std::result_of<F(Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (context) {
        QTimer::singleShot(
            duration.count(), context, [pm = std::move(promise), t = std::move(func)]() mutable {
                try {
                    pm.setValue(Try<resultType>(t()));
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            });
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
auto async(QPointer<QObject> context, F &&f, Args &&...args) -> Future<void>
{
    using resultType = typename std::result_of<F(Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (context) {
        QObject obj;
        QObject::connect(
            &obj, &QObject::destroyed, context, [pm = std::move(promise), t = std::move(func)]() mutable {
                try {
                    t();
                    pm.setValue();
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            },
            Qt::QueuedConnection);
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
auto async(QPointer<QObject> context, std::chrono::milliseconds duration, F &&f, Args &&...args) -> Future<void>
{
    using resultType = typename std::result_of<F(Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (context) {
        QTimer::singleShot(
            duration.count(), context, [pm = std::move(promise), t = std::move(func)]() mutable {
                try {
                    t();
                    pm.setValue();
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            });
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
auto async(Scheduler *sched, F &&f, Args &&...args) -> Future<typename std::result_of<F(Args...)>::type>
{
    using resultType = typename std::result_of<F(Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (sched) {
        sched->schedule([pm = std::move(promise), t = std::move(func)]() mutable {
            try {
                pm.setValue(Try<resultType>(t()));
            } catch (...) {
                pm.setException(std::current_exception());
            }
        });
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
auto async(Scheduler *sched, std::chrono::milliseconds duration, F &&f, Args &&...args) -> Future<typename std::result_of<F(Args...)>::type>
{
    using resultType = typename std::result_of<F(Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (sched) {
        sched->scheduleLater(duration, [pm = std::move(promise), t = std::move(func)]() mutable {
            try {
                pm.setValue(Try<resultType>(t()));
            } catch (...) {
                pm.setException(std::current_exception());
            }
        });
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
auto async(Scheduler *sched, F &&f, Args &&...args) -> Future<void>
{
    using resultType = typename std::result_of<F(Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (sched) {
        sched->schedule([pm = std::move(promise), t = std::move(func)]() mutable {
            try {
                t();
                pm.setValue();
            } catch (...) {
                pm.setException(std::current_exception());
            }
        });
    }
    return future;
}

template<typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
auto async(Scheduler *sched, std::chrono::milliseconds duration, F &&f, Args &&...args) -> Future<void>
{
    using resultType = typename std::result_of<F(Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    if (sched) {
        sched->scheduleLater(duration, [pm = std::move(promise), t = std::move(func)]() mutable {
            try {
                t();
                pm.setValue();
            } catch (...) {
                pm.setException(std::current_exception());
            }
        });
    }
    return future;
}

} // namespace Async

#endif // ASYNC_H
