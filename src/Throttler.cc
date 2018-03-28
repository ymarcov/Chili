#include "Throttler.h"

#include <algorithm>
#include <utility>

using namespace std::chrono;

namespace Chili {

Throttler::Throttler(std::size_t capacity, milliseconds interval)
    : _enabled(true)
    , _capacity(capacity)
    , _interval(std::move(interval))
    , _currentQuota(capacity) {}

Throttler::Throttler(const Throttler& other) {
    *this = other;
}

Throttler& Throttler::operator=(const Throttler& rhs) {
    std::lock_guard<std::mutex> lock1(_mutex);
    std::lock_guard<std::mutex> lock2(rhs._mutex);
    _enabled = rhs._enabled;
    _capacity = rhs._capacity;
    _interval = rhs._interval;
    _currentQuota = rhs._currentQuota;
    _lastConsumption = rhs._lastConsumption;
    return *this;
}

bool Throttler::IsEnabled() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _enabled;
}

Clock::TimePoint Throttler::GetFillTime() const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return Clock::GetCurrentTime();

    UpdateCurrentQuota();

    auto ratio = _currentQuota / double(_capacity);
    auto remainingMs = milliseconds(std::size_t(_interval.count() * (1 - ratio)));

    return Clock::GetCurrentTime() + remainingMs;
}

Clock::TimePoint Throttler::GetFillTime(std::size_t desiredQuota) const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return Clock::GetCurrentTime();

    UpdateCurrentQuota();

    auto ratio = _currentQuota / double(_capacity);
    auto remainingMsTillFull = milliseconds(std::size_t(_interval.count() * (1 - ratio)));
    auto desiredRatio = desiredQuota / double(_capacity);
    auto timeout = std::size_t(remainingMsTillFull.count() * desiredRatio);

    return Clock::GetCurrentTime() + std::chrono::milliseconds(timeout);
}

std::size_t Throttler::GetCurrentQuota() const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return _capacity;

    return UpdateCurrentQuota();
}

std::size_t Throttler::GetCapacity() const {
    return _capacity;
}

void Throttler::Consume(std::size_t n) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return;

    UpdateCurrentQuota();

    if (n > _currentQuota)
        _currentQuota = 0;
    else
        _currentQuota -= n;

    _lastConsumption = Clock::GetCurrentTime();
}

std::size_t Throttler::UpdateCurrentQuota() const {
    auto elapsed = duration_cast<decltype(_interval)>(Clock::GetCurrentTime() - _lastConsumption);
    auto fillFactor = elapsed.count() / double(_interval.count());
    auto fill = std::size_t(_capacity * fillFactor);
    _currentQuota = std::min(_capacity, _currentQuota + fill);
    return _currentQuota;
}

} // namespace Chili

