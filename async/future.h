#ifndef FUTURE_H
#define FUTURE_H

#include <atomic>
#include <mutex>
#include <functional>
#include <type_traits>
#include <condition_variable>

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QThread>

#include "helper.h"
#include "try.h"
#include "scheduler.h"

namespace Async {

namespace internal {

enum class Progress {
    None,
    Timeout,
    Done,
    Retrieved,
};

using TimeoutCallback = std::function<void()>;

template<typename T>
struct State
{
    static_assert(std::is_same<T, void>::value || std::is_copy_constructible<T>() || std::is_move_constructible<T>(),
                  "must be copyable or movable or void");

    State()
        : progress_(Progress::None)
        , retrieved_{false}
    {
    }

    std::mutex thenLock_;

    using ValueType = typename TryWrapper<T>::Type;
    ValueType value_;
    std::function<void(ValueType &&)> then_;
    Progress progress_;

    std::function<void(TimeoutCallback &&)> onTimeout_;
    std::atomic<bool> retrieved_;

    bool isRoot() const
    {
        return !onTimeout_;
    }
};

} // end namespace internal

template<typename T>
class Future;

using namespace internal;

template<typename T>
class Promise
{
public:
    Promise()
        : state_(std::make_shared<State<T>>())
    {
    }

    // The lambda with movable capture can not be stored in
    // std::function, just for compile, do NOT copy promise!
    Promise(const Promise &) = default;
    Promise &operator=(const Promise &) = default;

    Promise(Promise &&pm) = default;
    Promise &operator=(Promise &&pm) = default;

    void setException(std::exception_ptr exp)
    {
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        bool isRoot = state_->isRoot();
        if (isRoot && state_->progress_ != Progress::None)
            return;

        state_->progress_ = Progress::Done;
        state_->value_ = typename State<T>::ValueType(std::move(exp));
        guard.unlock();

        if (state_->then_)
            state_->then_(std::move(state_->value_));
    }

    template<typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    setValue(SHIT &&t)
    {
        // If ThenImp is running, here will wait for the lock.
        // After set then_, ThenImp will release lock.
        // And this func got lock, definitely will call then_.
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        bool isRoot = state_->isRoot();
        if (isRoot && state_->progress_ != Progress::None)
            return;

        state_->progress_ = Progress::Done;
        state_->value_ = std::forward<SHIT>(t);

        guard.unlock();

        // When reach here, state_ is determined, so mutex is useless
        // If the ThenImp function run, it'll see the Done state and
        // call user func there, not assign to then_.
        if (state_->then_)
            state_->then_(std::move(state_->value_));
    }

    template<typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    setValue()
    {
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ != Progress::None)
            return;

        state_->progress_ = Progress::Done;
        state_->value_ = Try<void>();

        guard.unlock();
        if (state_->then_)
            state_->then_(std::move(state_->value_));
    }

    Future<T> getFuture()
    {
        bool expect = false;
        if (!state_->retrieved_.compare_exchange_strong(expect, true)) {
            throw std::runtime_error("Future already retrieved");
        }

        return Future<T>(state_);
    }

    bool isReady() const
    {
        return state_->progress_ != Progress::None;
    }

private:
    std::shared_ptr<State<T>> state_;
};

template<typename T2>
Future<T2> makeExceptionFuture(std::exception_ptr &&);

template<typename T>
class Future
{
public:
    using InnerType = T;

    template<typename U>
    friend class Future;

    Future() = default;

    Future(const Future &) = delete;
    void operator=(const Future &) = delete;

    Future(Future &&fut) = default;
    Future &operator=(Future &&fut) = default;

    explicit Future(std::shared_ptr<State<T>> state)
        : state_(std::move(state))
    {
    }

    bool
    valid() const
    {
        return state_ != nullptr;
    }

    // The blocking interface
    // PAY ATTENTION to deadlock: Wait thread must NOT be same as promise thread!!!
    typename State<T>::ValueType
    wait(const std::chrono::milliseconds &timeout = std::chrono::milliseconds(24 * 3600 * 1000))
    {
        std::unique_lock<std::mutex> guard(state_->thenLock_);
        switch (state_->progress_) {
        case Progress::None:
            break;

        case Progress::Timeout:
            throw std::runtime_error("Future timeout");

        case Progress::Done:
            state_->progress_ = Progress::Retrieved;
            return std::move(state_->value_);

        default:
            throw std::runtime_error("Future already retrieved");
        }
        guard.unlock();

        auto cond(std::make_shared<std::condition_variable>());
        auto mutex(std::make_shared<std::mutex>());
        bool ready = false;
        typename State<T>::ValueType value;

        this->then([&value, &ready, wcond = std::weak_ptr<std::condition_variable>(cond), wmutex = std::weak_ptr<std::mutex>(mutex)](typename State<T>::ValueType &&v) {
            auto cond = wcond.lock();
            auto mutex = wmutex.lock();
            if (!cond || !mutex)
                return;

            std::unique_lock<std::mutex> guard(*mutex);
            value = std::move(v);
            ready = true;
            cond->notify_one();
        });

        std::unique_lock<std::mutex> waiter(*mutex);
        bool success = cond->wait_for(waiter, timeout, [&ready]() { return ready; });
        if (success)
            return std::move(value);
        else
            throw std::runtime_error("Future wait_for timeout");
    }

    // T is of type Future<InnerType>
    template<typename SHIT = T>
    typename std::enable_if<IsFuture<SHIT>::value, SHIT>::type
    unwrap()
    {
        using InnerType = typename IsFuture<SHIT>::Inner;

        static_assert(std::is_same<SHIT, Future<InnerType>>::value, "Kidding me?");

        Promise<InnerType> prom;
        Future<InnerType> fut = prom.getFuture();

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {
            try {
                auto innerFuture = std::move(state_->value_);
                return std::move(innerFuture.value());
            } catch (const std::exception &) {
                return makeExceptionFuture<InnerType>(std::current_exception());
            }
        } else {
            _setCallback([pm = std::move(prom)](typename TryWrapper<SHIT>::Type &&innerFuture) mutable {
                try {
                    SHIT future = std::move(innerFuture.value());
                    future.then([pm = std::move(pm)](typename TryWrapper<InnerType>::Type &&t) mutable {
                        // No need scheduler here, think about this code:
                        // `outer.Unwrap().then(sched, func);`
                        // outer.Unwrap() is the inner future, the below line
                        // will trigger func in sched thread.
                        pm.setValue(std::move(t));
                    });
                } catch (...) {
                    pm.setException(std::current_exception());
                }
            });
        }

        return fut;
    }

    template<typename F,
             typename R = CallableResult<F, T>>
    auto then(F &&f) -> typename R::ReturnFutureType
    {
        Scheduler *tmp = nullptr;
        typedef typename R::Arg Arguments;
        return _thenImpl<F, R>(tmp, std::forward<F>(f), Arguments());
    }

    // 将回调函数转到context所在线程执行，需要避免在context所在线程wait当前future
    template<typename F,
             typename R = CallableResult<F, T>>
    auto then(QPointer<QObject> context, F &&f) -> typename R::ReturnFutureType
    {
        typedef typename R::Arg Arguments;
        return _thenImpl<F, R>(context, std::forward<F>(f), Arguments());
    }

    // f will be called in sched
    template<typename F,
             typename R = CallableResult<F, T>>
    auto then(Scheduler *sched, F &&f) -> typename R::ReturnFutureType
    {
        typedef typename R::Arg Arguments;
        return _thenImpl<F, R>(sched, std::forward<F>(f), Arguments());
    }

    //1. F does not return future type
    template<typename F, typename R, typename... Args>
    typename std::enable_if<!R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _thenImpl(Scheduler *sched, F &&f, ResultOfWrapper<F, Args...>)
    {
        static_assert(sizeof...(Args) <= 1, "Then must take zero/one argument");

        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.getFuture();

        using FuncType = typename std::decay<F>::type;

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {
            typename TryWrapper<T>::Type t;
            try {
                t = std::move(state_->value_);
            } catch (const std::exception &) {
                t = (typename TryWrapper<T>::Type)(std::current_exception());
            }

            guard.unlock();

            if (sched) {
                sched->schedule([t = std::move(t),
                                 f = std::forward<FuncType>(f),
                                 pm = std::move(pm)]() mutable {
                    auto result = wrapWithTry(f, std::move(t));
                    pm.setValue(std::move(result));
                });
            } else {
                auto result = wrapWithTry(f, std::move(t));
                pm.setValue(std::move(result));
            }
        } else {
            // 1. set pm's timeout callback
            nextFuture._setOnTimeout([weak_parent = std::weak_ptr<State<T>>(state_)](TimeoutCallback &&cb) {
                auto parent = weak_parent.lock();
                if (!parent)
                    return;

                {
                    std::unique_lock<std::mutex> guard(parent->thenLock_);
                    // if parent future is Done, let it go down
                    if (parent->progress_ != Progress::None)
                        return;

                    parent->progress_ = Progress::Timeout;
                }

                if (!parent->isRoot())
                    parent->onTimeout_(std::move(cb)); // propogate to the root
                else
                    cb();
            });
            // 2. set this future's then callback
            _setCallback([sched,
                          func = std::forward<FuncType>(f),
                          prom = std::move(pm)](typename TryWrapper<T>::Type &&t) mutable {
                if (sched) {
                    sched->schedule([func = std::move(func),
                                     t = std::move(t),
                                     prom = std::move(prom)]() mutable {
                        // run callback, T can be void, thanks to folly
                        auto result = wrapWithTry(func, std::move(t));
                        // set next future's result
                        prom.setValue(std::move(result));
                    });
                } else {
                    // run callback, T can be void, thanks to folly Try<>
                    auto result = wrapWithTry(func, std::move(t));
                    // set next future's result
                    prom.setValue(std::move(result));
                }
            });
        }

        return std::move(nextFuture);
    }

    template<typename F, typename R, typename... Args>
    typename std::enable_if<!R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _thenImpl(QPointer<QObject> context, F &&f, ResultOfWrapper<F, Args...>)
    {
        static_assert(sizeof...(Args) <= 1, "Then must take zero/one argument");

        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.getFuture();

        using FuncType = typename std::decay<F>::type;

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {
            typename TryWrapper<T>::Type t;
            try {
                t = std::move(state_->value_);
            } catch (const std::exception &) {
                t = (typename TryWrapper<T>::Type)(std::current_exception());
            }

            guard.unlock();

            if (context) {
                if (context->thread() == QThread::currentThread()) {
                    auto result = wrapWithTry(f, std::move(t));
                    pm.setValue(std::move(result));
                } else {
                    QObject obj;
                    QObject::connect(
                        &obj, &QObject::destroyed, context, [t = std::move(t), f = std::forward<FuncType>(f), pm = std::move(pm)]() mutable {
                            auto result = wrapWithTry(f, std::move(t));
                            pm.setValue(std::move(result));
                        },
                        Qt::QueuedConnection);
                }
            }
        } else {
            // 1. set pm's timeout callback
            nextFuture._setOnTimeout([weak_parent = std::weak_ptr<State<T>>(state_)](TimeoutCallback &&cb) {
                auto parent = weak_parent.lock();
                if (!parent)
                    return;

                {
                    std::unique_lock<std::mutex> guard(parent->thenLock_);
                    // if parent future is Done, let it go down
                    if (parent->progress_ != Progress::None)
                        return;

                    parent->progress_ = Progress::Timeout;
                }

                if (!parent->isRoot())
                    parent->onTimeout_(std::move(cb)); // propogate to the root
                else
                    cb();
            });
            // 2. set this future's then callback
            _setCallback([context,
                          func = std::forward<FuncType>(f),
                          prom = std::move(pm)](typename TryWrapper<T>::Type &&t) mutable {
                if (context) {
                    if (context->thread() == QThread::currentThread()) {
                        // run callback, T can be void, thanks to folly
                        auto result = wrapWithTry(func, std::move(t));
                        // set next future's result
                        prom.setValue(std::move(result));
                    } else {
                        QObject obj;
                        QObject::connect(
                            &obj, &QObject::destroyed, context, [func = std::move(func), t = std::move(t), prom = std::move(prom)]() mutable {
                                // run callback, T can be void, thanks to folly
                                auto result = wrapWithTry(func, std::move(t));
                                // set next future's result
                                prom.setValue(std::move(result));
                            },
                            Qt::QueuedConnection);
                    }
                }
            });
        }

        return std::move(nextFuture);
    }

    //2. F return another future type
    template<typename F, typename R, typename... Args>
    typename std::enable_if<R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _thenImpl(Scheduler *sched, F &&f, ResultOfWrapper<F, Args...>)
    {
        static_assert(sizeof...(Args) <= 1, "Then must take zero/one argument");

        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.getFuture();

        using FuncType = typename std::decay<F>::type;

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {
            typename TryWrapper<T>::Type t;
            try {
                t = std::move(state_->value_);
            } catch (const std::exception &) {
                t = decltype(t)(std::current_exception());
            }

            guard.unlock();

            auto cb = [res = std::move(t),
                       f = std::forward<FuncType>(f),
                       prom = std::move(pm)]() mutable {
                // because func return another future: innerFuture, when innerFuture is done, nextFuture can be done
                decltype(f(res.template Get<Args>()...)) innerFuture;
                if (res.HasException()) {
                    // Failed if Args... is void
                    innerFuture = f(typename TryWrapper<typename std::decay<Args...>::type>::Type(res.Exception()));
                } else {
                    innerFuture = f(res.template Get<Args>()...);
                }

                if (!innerFuture.valid()) {
                    return;
                }

                std::unique_lock<std::mutex> guard(innerFuture.state_->thenLock_);
                if (innerFuture.state_->progress_ == Progress::Timeout) {
                    throw std::runtime_error("Wrong state : Timeout");
                } else if (innerFuture.state_->progress_ == Progress::Done) {
                    typename TryWrapper<FReturnType>::Type t;
                    try {
                        t = std::move(innerFuture.state_->value_);
                    } catch (const std::exception &) {
                        t = decltype(t)(std::current_exception());
                    }

                    guard.unlock();
                    prom.setValue(std::move(t));
                } else {
                    innerFuture._setCallback([prom = std::move(prom)](typename TryWrapper<FReturnType>::Type &&t) mutable {
                        prom.setValue(std::move(t));
                    });
                }
            };

            if (sched)
                sched->schedule(std::move(cb));
            else
                cb();
        } else {
            // 1. set pm's timeout callback
            nextFuture._setOnTimeout([weak_parent = std::weak_ptr<State<T>>(state_)](TimeoutCallback &&cb) {
                auto parent = weak_parent.lock();
                if (!parent)
                    return;

                {
                    std::unique_lock<std::mutex> guard(parent->thenLock_);
                    if (parent->progress_ != Progress::None)
                        return;

                    parent->progress_ = Progress::Timeout;
                }

                if (!parent->IsRoot())
                    parent->onTimeout_(std::move(cb)); // propogate to the root
                else
                    cb();
            });
            // 2. set this future's then callback
            _setCallback([sched = sched,
                          func = std::forward<FuncType>(f),
                          prom = std::move(pm)](typename TryWrapper<T>::Type &&t) mutable {
                auto cb = [func = std::move(func), t = std::move(t), prom = std::move(prom)]() mutable {
                    // because func return another future: innerFuture, when innerFuture is done, nextFuture can be done
                    decltype(func(t.template Get<Args>()...)) innerFuture;
                    if (t.HasException()) {
                        // Failed if Args... is void
                        innerFuture = func(typename TryWrapper<typename std::decay<Args...>::type>::Type(t.Exception()));
                    } else {
                        innerFuture = func(t.template Get<Args>()...);
                    }

                    if (!innerFuture.valid()) {
                        return;
                    }
                    std::unique_lock<std::mutex> guard(innerFuture.state_->thenLock_);
                    if (innerFuture.state_->progress_ == Progress::Timeout) {
                        throw std::runtime_error("Wrong state : Timeout");
                    } else if (innerFuture.state_->progress_ == Progress::Done) {
                        typename TryWrapper<FReturnType>::Type t;
                        try {
                            t = std::move(innerFuture.state_->value_);
                        } catch (const std::exception &) {
                            t = decltype(t)(std::current_exception());
                        }

                        guard.unlock();
                        prom.setValue(std::move(t));
                    } else {
                        innerFuture._setCallback([prom = std::move(prom)](typename TryWrapper<FReturnType>::Type &&t) mutable {
                            prom.setValue(std::move(t));
                        });
                    }
                };

                if (sched)
                    sched->schedule(std::move(cb));
                else
                    cb();
            });
        }

        return std::move(nextFuture);
    }

    template<typename F, typename R, typename... Args>
    typename std::enable_if<R::IsReturnsFuture::value, typename R::ReturnFutureType>::type
    _thenImpl(QPointer<QObject> context, F &&f, ResultOfWrapper<F, Args...>)
    {
        static_assert(sizeof...(Args) <= 1, "Then must take zero/one argument");

        using FReturnType = typename R::IsReturnsFuture::Inner;

        Promise<FReturnType> pm;
        auto nextFuture = pm.getFuture();

        using FuncType = typename std::decay<F>::type;

        std::unique_lock<std::mutex> guard(state_->thenLock_);
        if (state_->progress_ == Progress::Timeout) {
            throw std::runtime_error("Wrong state : Timeout");
        } else if (state_->progress_ == Progress::Done) {
            typename TryWrapper<T>::Type t;
            try {
                t = std::move(state_->value_);
            } catch (const std::exception &e) {
                t = decltype(t)(std::current_exception());
            }

            guard.unlock();

            auto cb = [res = std::move(t),
                       f = std::forward<FuncType>(f),
                       prom = std::move(pm)]() mutable {
                // because func return another future: innerFuture, when innerFuture is done, nextFuture can be done
                decltype(f(res.template Get<Args>()...)) innerFuture;
                if (res.HasException()) {
                    // Failed if Args... is void
                    innerFuture = f(typename TryWrapper<typename std::decay<Args...>::type>::Type(res.Exception()));
                } else {
                    innerFuture = f(res.template Get<Args>()...);
                }

                if (!innerFuture.valid()) {
                    return;
                }

                std::unique_lock<std::mutex> guard(innerFuture.state_->thenLock_);
                if (innerFuture.state_->progress_ == Progress::Timeout) {
                    throw std::runtime_error("Wrong state : Timeout");
                } else if (innerFuture.state_->progress_ == Progress::Done) {
                    typename TryWrapper<FReturnType>::Type t;
                    try {
                        t = std::move(innerFuture.state_->value_);
                    } catch (const std::exception &) {
                        t = decltype(t)(std::current_exception());
                    }

                    guard.unlock();
                    prom.setValue(std::move(t));
                } else {
                    innerFuture._setCallback([prom = std::move(prom)](typename TryWrapper<FReturnType>::Type &&t) mutable {
                        prom.setValue(std::move(t));
                    });
                }
            };

            if (context) {
                if (context->thread() == QThread::currentThread()) {
                    cb();
                } else {
                    QObject obj;
                    QObject::connect(
                        &obj, &QObject::destroyed, context, [cb = std::move(cb)]() mutable {
                            cb();
                        },
                        Qt::QueuedConnection);
                }
            }
        } else {
            // 1. set pm's timeout callback
            nextFuture._setOnTimeout([weak_parent = std::weak_ptr<State<T>>(state_)](TimeoutCallback &&cb) {
                auto parent = weak_parent.lock();
                if (!parent)
                    return;

                {
                    std::unique_lock<std::mutex> guard(parent->thenLock_);
                    if (parent->progress_ != Progress::None)
                        return;

                    parent->progress_ = Progress::Timeout;
                }

                if (!parent->IsRoot())
                    parent->onTimeout_(std::move(cb)); // propogate to the root
                else
                    cb();
            });
            // 2. set this future's then callback
            _setCallback([context = context,
                          func = std::forward<FuncType>(f),
                          prom = std::move(pm)](typename TryWrapper<T>::Type &&t) mutable {
                auto cb = [func = std::move(func), t = std::move(t), prom = std::move(prom)]() mutable {
                    // because func return another future: innerFuture, when innerFuture is done, nextFuture can be done
                    decltype(func(t.template Get<Args>()...)) innerFuture;
                    if (t.HasException()) {
                        // Failed if Args... is void
                        innerFuture = func(typename TryWrapper<typename std::decay<Args...>::type>::Type(t.Exception()));
                    } else {
                        innerFuture = func(t.template Get<Args>()...);
                    }

                    if (!innerFuture.valid()) {
                        return;
                    }
                    std::unique_lock<std::mutex> guard(innerFuture.state_->thenLock_);
                    if (innerFuture.state_->progress_ == Progress::Timeout) {
                        throw std::runtime_error("Wrong state : Timeout");
                    } else if (innerFuture.state_->progress_ == Progress::Done) {
                        typename TryWrapper<FReturnType>::Type t;
                        try {
                            t = std::move(innerFuture.state_->value_);
                        } catch (const std::exception &) {
                            t = decltype(t)(std::current_exception());
                        }

                        guard.unlock();
                        prom.setValue(std::move(t));
                    } else {
                        innerFuture._setCallback([prom = std::move(prom)](typename TryWrapper<FReturnType>::Type &&t) mutable {
                            prom.setValue(std::move(t));
                        });
                    }
                };

                if (context) {
                    if (context->thread() == QThread::currentThread()) {
                        cb();
                    } else {
                        QObject obj;
                        QObject::connect(
                            &obj, &QObject::destroyed, context, [cb = std::move(cb)]() mutable {
                                cb();
                            },
                            Qt::QueuedConnection);
                    }
                }
            });
        }

        return std::move(nextFuture);
    }

    /*
     * When register callbacks and timeout for a future like this:
     *      Future<int> f;
     *      f.then(xx).onTimeout(yy);
     *
     * There will be one future object created except f, we call f as root future.
     * The yy callback is registed on the last future, here are the possiblities:
     * 1. xx is called, and yy is not called.
     * 2. xx is not called, and yy is called.
     *
     * BUT BE CAREFUL BELOW:
     *
     *      Future<int> f;
     *      f.then(xx).then(yy).onTimeout(zz);
     *
     * There will be 3 future objects created except f, we call f as root future.
     * The zz callback is registed on the last future, here are the possiblities:
     * 1. xx is called, and zz is called, yy is not called.
     * 2. xx and yy are called, and zz is called, aha, it's rarely happend but...
     * 3. xx and yy are called, it's the normal case.
     * So, you may shouldn't use onTimeout with chained futures!!!
     */
    void onTimeout(std::chrono::milliseconds duration,
                   TimeoutCallback f,
                   Scheduler *scheduler)
    {
        scheduler->scheduleLater(duration, [state = state_, cb = std::move(f)]() mutable {
            {
                std::unique_lock<std::mutex> guard(state->thenLock_);

                if (state->progress_ != Progress::None)
                    return;

                state->progress_ = Progress::Timeout;
            }

            if (!state->IsRoot())
                state->onTimeout_(std::move(cb)); // propogate to the root future
            else
                cb();
        });
    }

    void onTimeout(std::chrono::milliseconds duration,
                   TimeoutCallback f,
                   QPointer<QObject> context)
    {
        QTimer::singleShot(
            duration, context, [state = state_, cb = std::move(f)]() mutable {
                {
                    std::unique_lock<std::mutex> guard(state->thenLock_);

                    if (state->progress_ != Progress::None)
                        return;

                    state->progress_ = Progress::Timeout;
                }

                if (!state->isRoot())
                    state->onTimeout_(std::move(cb)); // propogate to the root future
                else
                    cb();
            });
    }

private:
    void _setCallback(std::function<void(typename TryWrapper<T>::Type &&)> &&func)
    {
        state_->then_ = std::move(func);
    }

    void _setOnTimeout(std::function<void(TimeoutCallback &&)> &&func)
    {
        state_->onTimeout_ = std::move(func);
    }

    std::shared_ptr<State<T>> state_;
};

// Make ready future
template<typename T2>
inline Future<typename std::decay<T2>::type> makeReadyFuture(T2 &&value)
{
    Promise<typename std::decay<T2>::type> pm;
    auto f(pm.getFuture());
    pm.setValue(std::forward<T2>(value));

    return f;
}

inline Future<void> makeReadyFuture()
{
    Promise<void> pm;
    auto f(pm.getFuture());
    pm.setValue();

    return f;
}

// Make exception future
template<typename T2, typename E>
inline Future<T2> makeExceptionFuture(E &&exp)
{
    Promise<T2> pm;
    pm.setException(std::make_exception_ptr(std::forward<E>(exp)));

    return pm.getFuture();
}

template<typename T2>
inline Future<T2> makeExceptionFuture(std::exception_ptr &&eptr)
{
    Promise<T2> pm;
    pm.setException(std::move(eptr));

    return pm.getFuture();
}

// When All
template<typename... FT>
typename CollectAllVariadicContext<typename std::decay<FT>::type::InnerType...>::FutureType
whenAll(FT &&...futures)
{
    auto ctx = std::make_shared<CollectAllVariadicContext<typename std::decay<FT>::type::InnerType...>>();

    collectVariadicHelper<CollectAllVariadicContext>(
        ctx, std::forward<typename std::decay<FT>::type>(futures)...);

    return ctx->pm.getFuture();
}

template<class InputIterator>
Future<
    std::vector<typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>
whenAll(InputIterator first, InputIterator last)
{
    using TryT = typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type;
    if (first == last)
        return makeReadyFuture(std::vector<TryT>());

    struct AllContext
    {
        AllContext(int n)
            : results(n)
        {}
        ~AllContext()
        {
            // I think this line is useless.
            // pm.setValue(std::move(results));
        }

        Promise<std::vector<TryT>> pm;
        std::vector<TryT> results;
        std::atomic<size_t> collected{0};
    };

    auto ctx = std::make_shared<AllContext>(std::distance(first, last));

    for (size_t i = 0; first != last; ++first, ++i) {
        first->then([ctx, i](TryT &&t) {
            ctx->results[i] = std::move(t);
            if (ctx->results.size() - 1 == std::atomic_fetch_add(&ctx->collected, std::size_t(1))) {
                ctx->pm.setValue(std::move(ctx->results));
            }
        });
    }

    return ctx->pm.getFuture();
}

// When Any
template<class InputIterator>
Future<
    std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>
whenAny(InputIterator first, InputIterator last)
{
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    if (first == last) {
        return makeReadyFuture(std::make_pair(size_t(0), TryT()));
    }

    struct AnyContext
    {
        AnyContext(){};
        Promise<std::pair<size_t, TryT>> pm;
        std::atomic<bool> done{false};
    };

    auto ctx = std::make_shared<AnyContext>();
    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i](TryT &&t) {
            if (!ctx->done.exchange(true)) {
                ctx->pm.setValue(std::make_pair(i, std::move(t)));
            }
        });
    }

    return ctx->pm.getFuture();
}

// When N
template<class InputIterator>
Future<
    std::vector<std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>>
whenN(size_t N, InputIterator first, InputIterator last)
{
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    size_t nFutures = std::distance(first, last);
    const size_t needCollect = std::min<size_t>(nFutures, N);

    if (needCollect == 0) {
        return makeReadyFuture(std::vector<std::pair<size_t, TryT>>());
    }

    struct NContext
    {
        NContext(size_t _needs)
            : needs(_needs)
        {}
        Promise<std::vector<std::pair<size_t, TryT>>> pm;

        std::mutex mutex;
        std::vector<std::pair<size_t, TryT>> results;
        const size_t needs;
        bool done{false};
    };

    auto ctx = std::make_shared<NContext>(needCollect);
    for (size_t i = 0; first != last; ++first, ++i) {
        first->Then([ctx, i](TryT &&t) {
            std::unique_lock<std::mutex> guard(ctx->mutex);
            if (ctx->done)
                return;

            ctx->results.push_back(std::make_pair(i, std::move(t)));
            if (ctx->needs == ctx->results.size()) {
                ctx->done = true;
                guard.unlock();
                ctx->pm.setValue(std::move(ctx->results));
            }
        });
    }

    return ctx->pm.getFuture();
}

// When If Any
template<class InputIterator>
Future<
    std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>
whenIfAny(InputIterator first, InputIterator last, std::function<bool(const Try<typename std::iterator_traits<InputIterator>::value_type::InnerType> &)> cond)
{
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    if (first == last) {
        return makeReadyFuture(std::make_pair(size_t(0), TryT()));
    }

    const size_t nFutures = std::distance(first, last);

    struct IfAnyContext
    {
        IfAnyContext(){};
        Promise<std::pair<size_t, TryT>> pm;
        std::atomic<size_t> returned{0}; // including fail response, eg, cond(rsp) == false
        std::atomic<bool> done{false};
    };

    auto ctx = std::make_shared<IfAnyContext>();
    for (size_t i = 0; first != last; ++first, ++i) {
        first->then([ctx, i, nFutures, cond](TryT &&t) {
            if (ctx->done) {
                ctx->returned.fetch_add(1);
                return;
            }
            if (!cond(t)) {
                const size_t returned = ctx->returned.fetch_add(1) + 1;
                if (returned == nFutures) {
                    // If some success future done, below if statement will be false
                    if (!ctx->done.exchange(true)) {
                        // FAILED...
                        try {
                            throw std::runtime_error("WhenIfAny Failed, no true condition.");
                        } catch (...) {
                            ctx->pm.setException(std::current_exception());
                        }
                    }
                }

                return;
            }
            if (!ctx->done.exchange(true)) {
                ctx->pm.setValue(std::make_pair(i, std::move(t)));
            }
            ctx->returned.fetch_add(1);
        });
    }

    return ctx->pm.getFuture();
}

// When if N
template<class InputIterator>
Future<
    std::vector<std::pair<size_t, typename TryWrapper<typename std::iterator_traits<InputIterator>::value_type::InnerType>::Type>>>
whenIfN(size_t N, InputIterator first, InputIterator last, std::function<bool(const Try<typename std::iterator_traits<InputIterator>::value_type::InnerType> &)> cond)
{
    using T = typename std::iterator_traits<InputIterator>::value_type::InnerType;
    using TryT = typename TryWrapper<T>::Type;

    size_t nFutures = std::distance(first, last);
    const size_t needCollect = std::min<size_t>(nFutures, N);

    if (needCollect == 0) {
        return makeReadyFuture(std::vector<std::pair<size_t, TryT>>());
    }

    struct IfNContext
    {
        IfNContext(size_t _needs)
            : needs(_needs)
        {}
        Promise<std::vector<std::pair<size_t, TryT>>> pm;

        std::mutex mutex;
        std::vector<std::pair<size_t, TryT>> results;
        size_t returned{0}; // including fail response, eg, cond(rsp) == false
        const size_t needs;
        bool done{false};
    };

    auto ctx = std::make_shared<IfNContext>(needCollect);
    for (size_t i = 0; first != last; ++first, ++i) {
        first->then([ctx, i, nFutures, cond](TryT &&t) {
            std::unique_lock<std::mutex> guard(ctx->mutex);
            ++ctx->returned;
            if (ctx->done)
                return;

            if (!cond(t)) {
                if (ctx->returned == nFutures) {
                    // Failed: all returned, but not enough true cond(t)!
                    // Should I return partial result ???
                    try {
                        throw std::runtime_error("WhenIfN Failed, not enough true condition.");
                    } catch (...) {
                        ctx->done = true;
                        guard.unlock();
                        ctx->pm.setException(std::current_exception());
                    }
                }

                return;
            }

            ctx->results.push_back(std::make_pair(i, std::move(t)));
            if (ctx->needs == ctx->results.size()) {
                ctx->done = true;
                guard.unlock();
                ctx->pm.setValue(std::move(ctx->results));
            }
        });
    }

    return ctx->pm.getFuture();
}

} // namespace Async

#endif // FUTURE_H
