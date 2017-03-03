#pragma once

#include <chrono>
#include <cstddef>

namespace Yam {
namespace Http {

class Throttler {
public:
    using Clock = std::chrono::steady_clock;

    Throttler();
    Throttler(std::size_t capacity, std::chrono::milliseconds interval);

    std::chrono::time_point<Clock> GetFillTimePoint() const;
    std::size_t GetCurrentQuota() const;
    void Consume(std::size_t);

private:
    std::size_t UpdateCurrentQuota() const;

    bool _enabled = false;
    std::size_t _capacity;
    std::chrono::milliseconds _interval;
    std::chrono::time_point<Clock> _lastConsumption;
    mutable std::size_t _currentQuota;
};

} // namespace Http
} // namespace Yam

