#pragma once

#include <mutex>
#include <utility>

namespace Chili {

template <class T>
class Synchronized {
public:
    Synchronized() = default;

    template <class U>
    Synchronized(U&& u) {
        _t = std::forward<U>(u);
    }

    T& GetRefUnlocked() {
        return _t;
    }

    const T& GetRefUnlocked() const {
        return _t;
    }

    T GetCopy() const {
        std::lock_guard lock(_mutex);
        return _t;
    }

    template <class U>
    Synchronized& Set(U&& u) {
        std::lock_guard lock(_mutex);
        _t = std::forward<U>(u);
        return *this;
    }

    template <class Callable>
    decltype(auto) Synchronize(Callable&& c) {
        std::lock_guard lock(_mutex);
        return c(_t);
    }

private:
    T _t;
    mutable std::mutex _mutex;
};

} // namespace Chili

