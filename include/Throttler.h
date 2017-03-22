#pragma once

#include <chrono>
#include <cstddef>
#include <limits>
#include <mutex>

namespace Yam {
namespace Http {

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
    using Clock = std::chrono::steady_clock;

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
    std::chrono::time_point<Clock> GetFillTime() const;
    std::size_t GetCurrentQuota() const;
    void Consume(std::size_t);
    std::size_t GetCapacity() const;

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

