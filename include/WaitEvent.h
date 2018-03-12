#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Nitra {

class WaitEvent {
public:
    void Reset();
    void Signal();
    void Wait() const;
    bool TryWait() const;
    bool Wait(std::chrono::microseconds timeout) const;
    bool WaitUntil(std::chrono::time_point<std::chrono::steady_clock>) const;
    void WaitAndReset();
    bool TryWaitAndReset();
    bool WaitAndReset(std::chrono::microseconds timeout);
    bool WaitUntilAndReset(std::chrono::time_point<std::chrono::steady_clock>);

private:
    mutable std::mutex _mutex;
    mutable std::condition_variable _cv;
    bool _signalled = false;
};

} // namespace Nitra

