#include "WaitEvent.h"

namespace Yam {
namespace Http {

void WaitEvent::Reset() {
    _mutex.lock();
    _signalled = false;
    _mutex.unlock();
}

void WaitEvent::Signal() {
    _mutex.lock();
    _signalled = true;
    _mutex.unlock();
    _cv.notify_all();
}

void WaitEvent::Wait() const {
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] { return _signalled == true; });
}

bool WaitEvent::Wait(std::chrono::microseconds timeout) const {
    std::unique_lock<std::mutex> lock(_mutex);
    return _cv.wait_for(lock, timeout, [this] { return _signalled == true; });
}

} // namespace Http
} // namespace Yam

