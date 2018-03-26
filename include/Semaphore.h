#pragma once

#include "Clock.h"

#include <chrono>
#include <memory>

namespace Nitra {

class Semaphore {
public:
    Semaphore(unsigned initialValue = 0);
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    ~Semaphore();

    unsigned GetValue() const;
    void Increment();
    void Decrement();
    bool TryDecrement();
    bool TryDecrement(std::chrono::nanoseconds timeout);
    bool TryDecrementUntil(Clock::TimePoint timeout);

private:
    std::shared_ptr<void> _nativeHandle;
};

} // namespace Nitra

