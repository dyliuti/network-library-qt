#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "future.h"
#include "scheduler.h"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

///@file ThreadPool.h
///@brief A powerful ThreadPool implementation with Future interface.
/// Usage:
///@code
/// pool.Execute(your_heavy_work, some_args)
///     .then(process_heavy_work_result)
///@endcode
///
///  Here, your_heavy_work will be executed in a thread, and return Future
///  immediately. When it done, function process_heavy_work_result will be called.
///  The type of argument of process_heavy_work_result is the same as the return
///  type of your_heavy_work.
namespace Async {

///@brief A powerful ThreadPool implementation with Future interface.
class ThreadPool final : public Scheduler {
public:
    ThreadPool(int numThreads = 8);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    void operator=(const ThreadPool&) = delete;

    static ThreadPool* globalInstance();

    ///@brief Execute work in this pool
    ///@return A future, you can register callback on it
    /// when f is done or timeout.
    ///
    /// If the threads size not reach limit, or there are
    /// some idle threads, f will be executed at once.
    /// But if all threads are busy and threads size reach
    /// limit, f will be queueing, will be executed later.
    ///
    /// F returns non-void
    template <typename F, typename... Args, typename = typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type, typename Dummy = void>
    auto execute(F&& f, Args&&... args) -> Future<typename std::result_of<F(Args...)>::type>;

    ///@brief Execute work in this pool
    ///@return A future, you can register callback on it
    /// when f is done or timeout.
    ///
    /// If the threads size not reach limit, or there are
    /// some idle threads, f will be executed at once.
    /// But if all threads are busy and threads size reach
    /// limit, f will be queueing, will be executed later.
    ///
    /// F returns void
    template <typename F, typename... Args, typename = typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value, void>::type>
    auto execute(F&& f, Args&&... args) -> Future<void>;

    ///@brief Stop thread pool and wait all threads terminate
    void joinAll();

    ///@brief Set number of threads
    ///
    /// Num of threads is fixed after start thread pool
    /// Default value is 1
    void setNumOfThreads(int);

    // ---- below are for unittest ----
    // num of workers
    size_t workerThreads() const;
    // num of waiting tasks
    size_t tasks() const;

    virtual void scheduleLater(std::chrono::milliseconds duration, std::function<void()> f);

    virtual void schedule(std::function<void()> f);

private:
    void _workerRoutine();
    void _start();

    int numThreads_ { 1 };
    std::deque<std::thread> workers_;

    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool shutdown_ { false };
    std::deque<std::function<void()>> tasks_;

    static const int kMaxThreads = 512;
    static std::thread::id s_mainThread;
};

// if F return something
template <typename F, typename... Args, typename, typename>
auto ThreadPool::execute(F&& f, Args&&... args) -> Future<typename std::result_of<F(Args...)>::type>
{
    using resultType = typename std::result_of<F(Args...)>::type;

    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
        throw std::runtime_error("execute on closed thread pool");

    if (workers_.empty()) {
        _start();
    }

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = [t = std::move(func), pm = std::move(promise)]() mutable {
        try {
            pm.setValue(Try<resultType>(t()));
        } catch (...) {
            pm.setException(std::current_exception());
        }
    };

    tasks_.emplace_back(std::move(task));
    cond_.notify_one();

    return future;
}

// F return void
template <typename F, typename... Args, typename>
auto ThreadPool::execute(F&& f, Args&&... args) -> Future<void>
{
    using resultType = typename std::result_of<F(Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
        return makeReadyFuture();

    if (workers_.empty()) {
        _start();
    }

    Promise<resultType> promise;
    auto future = promise.getFuture();

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = [t = std::move(func), pm = std::move(promise)]() mutable {
        try {
            t();
            pm.setValue();
        } catch (...) {
            pm.setException(std::current_exception());
        }
    };

    tasks_.emplace_back(std::move(task));
    cond_.notify_one();

    return future;
}

} // namespace Async

#endif
