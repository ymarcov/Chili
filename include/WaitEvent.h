#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Yam {
namespace Http {

class WaitEvent {
public:
    void Signal();
    void Wait() const;
    void Wait(std::chrono::microseconds timeout) const;

private:
    mutable std::mutex _mutex;
    mutable std::condition_variable _cv;
    bool _signalled = false;
};

} // namespace Http
} // namespace Yam

