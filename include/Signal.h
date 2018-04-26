#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace Chili {

template <class... Param>
class Signal {
public:
    using Callback = std::function<void(Param...)>;

    template <class... Args>
    void operator()(Args&&... args) const {
        Raise(args...);
    }

    template <class... Args>
    void Raise(Args&&... args) const {
        for (auto& s : _subscribers)
            s(args...);
    }

    template <class T>
    Signal& operator+=(T&& f) {
        return Subscribe(std::forward<T>(f));
    }

    template <class T>
    Signal& Subscribe(T&& f) {
        _subscribers.push_back(std::forward<T>(f));
        return *this;
    }

private:
    std::vector<Callback> _subscribers;
};

template <class... Param>
class SynchronizedSignal {
public:
    using Callback = std::function<void(Param...)>;

    template <class... Args>
    void operator()(Args&&... args) const {
        Raise(args...);
    }

    template <class... Args>
    void Raise(Args&&... args) const {
        std::unique_lock lock(_mutex);
        auto subscribers = _subscribers;
        lock.unlock();

        for (auto& s : _subscribers)
            s(args...);
    }

    template <class T>
    SynchronizedSignal& operator+=(T&& f) {
        return Subscribe(std::forward<T>(f));
    }

    template <class T>
    SynchronizedSignal& Subscribe(T&& f) {
        std::lock_guard lock(_mutex);
        _subscribers.push_back(std::forward<T>(f));
        return *this;
    }

private:
    std::vector<Callback> _subscribers;
    mutable std::mutex _mutex;
};

} // namespace Chili

