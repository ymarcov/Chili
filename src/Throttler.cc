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

std::chrono::time_point<Throttler::Clock> Throttler::GetFillTimePoint() const {
    if (!_enabled)
        return Clock::now();

    UpdateCurrentQuota();

    auto ratio = _currentQuota / double(_capacity);
    auto remainingMs = milliseconds(std::size_t(_interval.count() * (1 - ratio)));

    return Clock::now() + remainingMs;
}

std::size_t Throttler::GetCurrentQuota() const {
    if (!_enabled)
        return _capacity;

    return UpdateCurrentQuota();
}

void Throttler::Consume(std::size_t n) {
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

