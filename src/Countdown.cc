#include "Countdown.h"

namespace Chili {

Countdown::Countdown(unsigned value) :
    _initialValue{value},
    _count{value} {}

unsigned Countdown::GetInitialValue() const {
    return _initialValue;
}

bool Countdown::Tick() {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_count == 0)
        return false;

    if (--_count == 0) {
        _event.Signal();
        return false;
    }

    return true;
}

void Countdown::Wait() const {
    _event.Wait();
}

bool Countdown::Wait(std::chrono::microseconds timeout) const {
    return _event.Wait(timeout);
}

} // namespace Chili

