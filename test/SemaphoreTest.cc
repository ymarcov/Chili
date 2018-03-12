#include <gmock/gmock.h>

#include "Semaphore.h"
#include "WaitEvent.h"

#include <chrono>
#include <thread>

using namespace ::testing;
using namespace std::literals;

namespace Nitra {

class SemaphoreTest : public Test {
public:
    SemaphoreTest() {
    }

protected:
};

TEST_F(SemaphoreTest, basic_usage) {
    WaitEvent threadReady;
    Semaphore s;

    std::thread thread([&] {
            threadReady.Signal();
            s.Increment();
    });

    threadReady.Wait();
    s.Decrement();
    thread.join();
}

TEST_F(SemaphoreTest, times_out) {
    Semaphore s;
    EXPECT_FALSE(s.TryDecrement(10ms));
    s.Increment();
    EXPECT_TRUE(s.TryDecrement(10ms));
}

TEST_F(SemaphoreTest, try_decrement) {
    Semaphore s;
    EXPECT_FALSE(s.TryDecrement());
    s.Increment();
    EXPECT_TRUE(s.TryDecrement());
}

TEST_F(SemaphoreTest, multiple) {
    Semaphore s;
    const auto count = 5;
    std::vector<std::thread> threads(count);
    std::vector<WaitEvent> ready(count);

    for (int i = 0; i < count; i++) {
        threads[i] = std::thread([&s, &ready, i] {
            ready[i].Signal();
            s.Decrement();
        });
    }

    for (int i = 0; i < count; i++)
        ready[i].Wait();

    for (int i = 0; i < count; i++)
        s.Increment();

    for (int i = 0; i < count; i++)
        threads[i].join();
}

} // namespace Nitra

