#ifndef SHAREDPROMISE_H
#define SHAREDPROMISE_H

#include "future.h"

namespace Async {

template<typename T>
class SharedPromise
{
public:
    SharedPromise()
        : _value()
        , _hasValue(false)
    {
    }

    SharedPromise(const SharedPromise &) = default;
    SharedPromise &operator=(const SharedPromise &) = default;

    SharedPromise(SharedPromise &&pm) = default;
    SharedPromise &operator=(SharedPromise &&pm) = default;

    void setException(std::exception_ptr exp)
    {
        setTry(typename State<T>::ValueType(std::move(exp)));
    }

    template<typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    setValue(SHIT &&t)
    {
        setTry(std::forward<SHIT>(t));
    }

    template<typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    setValue()
    {
        setTry();
    }

    template<typename SHIT = T>
    typename std::enable_if<!std::is_void<SHIT>::value, void>::type
    setTry(SHIT &&t)
    {
        std::vector<Promise<T>> promises;

        {
            std::lock_guard<std::mutex> g(_mutex);
            if (hasResult()) {
                throw std::runtime_error("Future already retrieved");
            }
            _hasValue = true;
            _value = std::forward<SHIT>(t);
            promises.swap(_promises);
        }

        for (auto &p : promises) {
            if (_value.hasException())
                p.setException(_value.exception());
            else
                p.setValue(Try<T>(_value.value()));
        }
    }

    template<typename SHIT = T>
    typename std::enable_if<std::is_void<SHIT>::value, void>::type
    setTry()
    {
        std::vector<Promise<T>> promises;

        {
            std::lock_guard<std::mutex> g(_mutex);
            _hasValue = true;
            promises.swap(_promises);
        }

        for (auto &p : promises) {
            if (_value.hasException())
                p.setException(_value.exception());
            else
                p.setValue();
        }
    }

    template<typename SHIT = T,
             typename = typename std::enable_if<!std::is_void<SHIT>::value, void>::type>
    auto getFuture() -> Future<SHIT>
    {
        std::lock_guard<std::mutex> g(_mutex);
        if (hasResult()) {
            return makeReadyFuture<T>(Try<T>(_value.value()));
        } else {
            _promises.emplace_back();
            return _promises.back().getFuture();
        }
    }

    template<typename SHIT = T,
             typename = typename std::enable_if<std::is_void<SHIT>::value, void>::type>
    auto getFuture() -> Future<void>
    {
        std::lock_guard<std::mutex> g(_mutex);
        if (hasResult()) {
            return makeReadyFuture();
        } else {
            _promises.emplace_back();
            return _promises.back().getFuture();
        }
    }

protected:
    bool hasResult() const
    {
        return _hasValue;
    }

private:
    struct Mutex : std::mutex
    {
        Mutex() = default;

        Mutex(const Mutex &) {}
        Mutex &operator=(const Mutex &) { return *this; }

        Mutex(Mutex &&) noexcept {}
        Mutex &operator=(Mutex &&) noexcept { return *this; }
    };

    Mutex _mutex;
    using ValueType = typename TryWrapper<T>::Type;
    ValueType _value;
    bool _hasValue;
    std::vector<Promise<T>> _promises;
};

} // namespace Async

#endif // SHAREDPROMISE_H
