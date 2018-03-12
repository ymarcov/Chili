#pragma once

/**
 * @cond INTERNAL
 */

#include <atomic>

namespace Nitra {

/**
 * Just a regular spinlock.
 *
 * Satisfies the Mutex concept, and
 * can be used with std::lock_guard.
 */
class Spinlock {
public:
    Spinlock();
    Spinlock(const Spinlock&) = delete;

    bool TryLock(std::size_t jiffies = 1000);
    void Lock();
    void Unlock();

    void lock();
    void unlock();

private:
    std::atomic_flag _flag;
};

inline Spinlock::Spinlock() :
    _flag{ATOMIC_FLAG_INIT} {}

inline void Spinlock::lock() {
    Lock();
}

inline void Spinlock::unlock() {
    Unlock();
}

inline bool Spinlock::TryLock(std::size_t jiffies) {
    while (--jiffies)
        if (!_flag.test_and_set(std::memory_order_acquire))
            return true;
    return !_flag.test_and_set(std::memory_order_acquire);
}

inline void Spinlock::Lock() {
    while (_flag.test_and_set(std::memory_order_acquire))
        ;
}

inline void Spinlock::Unlock() {
    _flag.clear(std::memory_order_release);
}

} // namespace Nitra

/**
 * @endcond INTERNAL
 */
