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

    operator T&() {
        std::lock_guard lock(_mutex);
        return _t;
    }

    operator const T&() const {
        std::lock_guard lock(_mutex);
        return _t;
    }

    T& GetValue() {
        return _t;
    }

    const T& GetValue() const {
        std::lock_guard lock(_mutex);
        return _t;
    }

    template <class U>
    Synchronized& operator=(U&& u) {
        std::lock_guard lock(_mutex);
        _t = std::forward<U>(u);
        return *this;
    }

private:
    T _t;
    mutable std::mutex _mutex;
};

} // namespace Chili

