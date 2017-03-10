#include "Throttler.h"

#include <algorithm>
#include <utility>

using namespace std::chrono;

namespace Yam {
namespace Http {

Throttler::Throttler(std::size_t capacity, milliseconds interval)
    : _enabled(true)
    , _capacity(capacity)
    , _interval(std::move(interval))
    , _currentQuota(capacity) {}

Throttler::Throttler(const Throttler& other) {
    *this = other;
}

Throttler& Throttler::operator=(const Throttler& rhs) {
    std::lock_guard<std::mutex> lock(_mutex);
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

std::chrono::time_point<Throttler::Clock> Throttler::GetFillTime() const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return Clock::now();

    UpdateCurrentQuota();

    auto ratio = _currentQuota / double(_capacity);
    auto remainingMs = milliseconds(std::size_t(_interval.count() * (1 - ratio)));

    return Clock::now() + remainingMs;
}

std::size_t Throttler::GetCurrentQuota() const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return _capacity;

    return UpdateCurrentQuota();
}

void Throttler::Consume(std::size_t n) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_enabled)
        return;

    UpdateCurrentQuota();

    _currentQuota -= n;
    _lastConsumption = Clock::now();
}

std::size_t Throttler::UpdateCurrentQuota() const {
    auto elapsed = duration_cast<decltype(_interval)>(Clock::now() - _lastConsumption);
    auto fillFactor = elapsed.count() / double(_interval.count());
    auto fill = std::size_t(_capacity * fillFactor);
    _currentQuota = std::min(_capacity, _currentQuota + fill);
    return _currentQuota;
}

} // namespace Http
} // namespace Yam

