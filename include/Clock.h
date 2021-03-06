#pragma once

#include <chrono>

namespace Chili {

class Clock {
    using Impl = std::chrono::steady_clock;

public:
    using TimePoint = std::chrono::time_point<Impl>;

    static TimePoint GetCurrentTime();
};

inline Clock::TimePoint Clock::GetCurrentTime() {
    return Impl::now();
}

} // namespace Chili

