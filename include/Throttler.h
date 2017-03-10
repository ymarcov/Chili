#pragma once

#include <chrono>
#include <cstddef>
#include <limits>
#include <mutex>

namespace Yam {
namespace Http {

class Throttler {
public:
    using Clock = std::chrono::steady_clock;

    Throttler() = default;
    Throttler(std::size_t capacity, std::chrono::milliseconds interval);
    Throttler(const Throttler&);
    Throttler& operator=(const Throttler&);

    bool IsEnabled() const;
    std::chrono::time_point<Clock> GetFillTime() const;
    std::size_t GetCurrentQuota() const;
    void Consume(std::size_t);

private:
    std::size_t UpdateCurrentQuota() const;

    bool _enabled = false;
    mutable std::mutex _mutex;
    std::size_t _capacity = std::numeric_limits<std::size_t>::max();
    std::chrono::milliseconds _interval;
    std::chrono::time_point<Clock> _lastConsumption;
    mutable std::size_t _currentQuota;
};

} // namespace Http
} // namespace Yam

