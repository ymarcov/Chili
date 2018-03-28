#include "Semaphore.h"
#include "SystemError.h"

#include <semaphore.h>
#include <time.h>

namespace Chili {

Semaphore::Semaphore(unsigned initialValue) {
    _nativeHandle = std::make_shared<sem_t>();

    auto ret = ::sem_init(static_cast<sem_t*>(_nativeHandle.get()), 0, initialValue);

    if (ret == -1)
        throw SystemError();
}

Semaphore::~Semaphore() {
    ::sem_destroy(static_cast<sem_t*>(_nativeHandle.get()));
}

unsigned Semaphore::GetValue() const {
    int value;
    auto ret = ::sem_getvalue(static_cast<sem_t*>(_nativeHandle.get()), &value);

    if (ret == -1)
        throw SystemError();

    return value;
}

void Semaphore::Increment() {
    if (::sem_post(static_cast<sem_t*>(_nativeHandle.get())) == -1)
        throw SystemError();
}

void Semaphore::Decrement() {
    if (::sem_wait(static_cast<sem_t*>(_nativeHandle.get())) == -1)
        throw SystemError();
}

bool Semaphore::TryDecrement(std::chrono::nanoseconds timeout) {
    timespec ts;

    if (::clock_gettime(CLOCK_REALTIME, &ts) == -1)
        throw SystemError();

    ts.tv_sec += timeout.count() / int(1e9);

    auto addedNs = timeout.count() % int(1e9);

    if (ts.tv_nsec + addedNs >= int(1e9)) {
        ts.tv_sec += 1;
        ts.tv_nsec = (ts.tv_nsec + addedNs) % int(1e9);
    } else {
        ts.tv_nsec += timeout.count() % int(1e9);
    }

    auto ret = ::sem_timedwait(static_cast<sem_t*>(_nativeHandle.get()), &ts);

    if (ret == -1) {
        if (errno == ETIMEDOUT)
            return false;
        else
            throw SystemError();
    }

    return true;
}

bool Semaphore::TryDecrementUntil(Clock::TimePoint timeout) {
    auto now = Clock::GetCurrentTime();

    if (now >= timeout)
        return TryDecrement();

    return TryDecrement(timeout - now);
}

bool Semaphore::TryDecrement() {
    auto ret = ::sem_trywait(static_cast<sem_t*>(_nativeHandle.get()));

    if (ret == -1) {
        if (errno == EAGAIN)
            return false;

        throw SystemError();
    }

    return true;
}

} // namespace Chili

