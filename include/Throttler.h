#pragma once

#include "Clock.h"

#include <chrono>
#include <cstddef>
#include <limits>
#include <mutex>

namespace Nitra {

/**
 * Throttles I/O operations.
 *
 * This is useful when you want to do load-balancing
 * with regards to download/upload speeds, but also
 * when you're trying to stay under a certain rate
 * of bandwidth.
 */
class Throttler {
public:
    /**
     * Creates a disabled throttler that does not in fact throttle anything.
     */
    Throttler() = default;

    /**
     * Creates a throttler that only allows the specified
     * capacity to be read or written (depending on where it
     * is used) during every specified interval of time.
     */
    Throttler(std::size_t capacity, std::chrono::milliseconds interval);

    Throttler(const Throttler&);
    Throttler& operator=(const Throttler&);

    bool IsEnabled() const;
    Clock::TimePoint GetFillTime() const;
    Clock::TimePoint GetFillTime(std::size_t desiredQuota) const;
    std::size_t GetCurrentQuota() const;
    void Consume(std::size_t);
    std::size_t GetCapacity() const;

private:
    std::size_t UpdateCurrentQuota() const;

    bool _enabled = false;
    mutable std::mutex _mutex;
    std::size_t _capacity = std::numeric_limits<std::size_t>::max();
    std::chrono::milliseconds _interval;
    Clock::TimePoint _lastConsumption;
    mutable std::size_t _currentQuota;
};

} // namespace Nitra

