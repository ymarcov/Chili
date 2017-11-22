#pragma once

#include "WaitEvent.h"

#include <atomic>
#include <mutex>

namespace Yam {
namespace Http {

class Countdown {
public:
    /**
     * Initializes a countdown with the specified
     * number of decrements needed to reach zero.
     */
    Countdown(unsigned value);

    /**
     * Returns the initial value of the countdown.
     */
    unsigned GetInitialValue() const;

    /**
     * Decrements the current value.
     * Returns false if the count is at or has reached zero.
     */
    bool Tick();

    /**
     * Waits for the value to reach zero.
     */
    void Wait() const;

    /**
     * Waits for the value to reach zero, or for a timeout
     * to occur. If a timeout occurred, returns false,
     * otherwise returns true.
     */
    bool Wait(std::chrono::microseconds timeout) const;

private:
    unsigned _initialValue;
    unsigned _count;
    std::mutex _mutex;
    WaitEvent _event;
};

} // namespace Http
} // namespace Yam

