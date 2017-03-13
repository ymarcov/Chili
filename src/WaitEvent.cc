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

bool WaitEvent::WaitUntil(std::chrono::time_point<std::chrono::steady_clock> t) const {
    std::unique_lock<std::mutex> lock(_mutex);
    return _cv.wait_until(lock, t, [this] { return _signalled == true; });
}

void WaitEvent::WaitAndReset() {
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] { return _signalled == true; });
    _signalled = false;
}

bool WaitEvent::WaitAndReset(std::chrono::microseconds timeout) {
    std::unique_lock<std::mutex> lock(_mutex);

    if (_cv.wait_for(lock, timeout, [this] { return _signalled == true; })) {
        _signalled = false;
        return true;
    }

    return false;
}

bool WaitEvent::WaitUntilAndReset(std::chrono::time_point<std::chrono::steady_clock> t) {
    std::unique_lock<std::mutex> lock(_mutex);

    if (_cv.wait_until(lock, t, [this] { return _signalled == true; })) {
        _signalled = false;
        return true;
    }

    return false;
}

} // namespace Http
} // namespace Yam

