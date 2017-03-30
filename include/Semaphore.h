#pragma once

#include <chrono>
#include <memory>

namespace Yam {
namespace Http {

class Semaphore {
public:
    Semaphore(unsigned initialValue = 0);
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    ~Semaphore();

    unsigned GetValue() const;
    void Increment();
    void Decrement();
    bool TryDecrement(std::chrono::nanoseconds timeout);
    bool TryDecrement();

private:
    std::shared_ptr<void> _nativeHandle;
};

} // namespace Http
} // namespace Yam

