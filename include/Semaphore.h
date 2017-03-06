#pragma once

#include <chrono>
#include <memory>

namespace Yam {
namespace Http {

class Semaphore {
public:
    Semaphore(unsigned initialValue = 0);

    void Increment();
    void Decrement();
    bool Decrement(std::chrono::nanoseconds timeout);
    bool TryDecrement();

private:
    std::shared_ptr<void> _nativeHandle;
};

} // namespace Http
} // namespace Yam

