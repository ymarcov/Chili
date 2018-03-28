#pragma once

#include "Clock.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Chili {

class WaitEvent {
public:
    void Reset();
    void Signal();
    void Wait() const;
    bool TryWait() const;
    bool Wait(std::chrono::microseconds timeout) const;
    bool WaitUntil(Clock::TimePoint) const;
    void WaitAndReset();
    bool TryWaitAndReset();
    bool WaitAndReset(std::chrono::microseconds timeout);
    bool WaitUntilAndReset(Clock::TimePoint);

private:
    mutable std::mutex _mutex;
    mutable std::condition_variable _cv;
    bool _signalled = false;
};

} // namespace Chili

