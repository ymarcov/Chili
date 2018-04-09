#include <gmock/gmock.h>

#include "ThreadPool.h"

#include <thread>

using namespace ::testing;
using namespace std::literals;

namespace Chili {

class ThreadPoolTest : public Test {
};

TEST_F(ThreadPoolTest, increments_counter_concurrently) {
    const int threadCount = 10;
    ThreadPool tp{threadCount};
    std::vector<std::future<void>> futures;
    std::atomic_int counter{0};
    const auto target = 100000;

    tp.SetUpscalePatience(50us);

    for (int i = 0; i < target; i++) {
        futures.push_back(tp.Post([&] {
            ++counter;
        }));
    }

    for (auto& f : futures)
        f.get();

    ASSERT_EQ(target, counter);
}

TEST_F(ThreadPoolTest, autoscales) {
    ThreadPool tp{10};

    tp.SetUpscalePatience(10ms);
    tp.SetDownscalePatience(10ms);

    // T + 0ms
    ASSERT_EQ(0, tp.GetWorkerCount());

    auto t1 = tp.Post([&] {
        std::this_thread::sleep_for(20ms);
    });

    // T + 0ms
    ASSERT_EQ(1, tp.GetWorkerCount());

    auto t2 = tp.Post([&] {
        std::this_thread::sleep_for(10ms);
    });

    // T + 0ms
    ASSERT_EQ(1, tp.GetWorkerCount());

    std::this_thread::sleep_for(5ms);

    // T + 5ms
    ASSERT_EQ(1, tp.GetWorkerCount());

    std::this_thread::sleep_for(7500us);

    // at this point t2 has been stuck in queue for,
    // too long, so t3 should trigger a new worker

    auto t3 = tp.Post([&] {
        std::this_thread::sleep_for(30ms);
    });

    // T + 12.5ms
    // t1 is 12.5ms into its sleep
    // t2 is 2.5ms into its sleep
    // t3 hasn't started yet
    ASSERT_EQ(2, tp.GetWorkerCount()) << "Upscale didn't occur";

    std::this_thread::sleep_for(9500us);

    // T + 22ms
    // t1 is done 2ms ago
    // t2 is done 2ms ago
    // t3 is 2ms into its sleep
    // only one thread should be working now, but 2 alive,
    // while one is simply waiting to downscale
    ASSERT_EQ(2, tp.GetWorkerCount()) << "Downscale occurred too fast";

    std::this_thread::sleep_for(20ms);

    // T + 42ms
    // t1 is done 22ms ago, its thread might be running t3
    // t2 is done 12ms ago, its thread might be running t3
    // t3 is 22ms into its sleep
    // one thread is running t3, but other thread should
    // have gone past its downscale patience while waiting
    ASSERT_EQ(1, tp.GetWorkerCount()) << "Downscale didn't occur";

    t1.get();
    t2.get();
    t3.get();
}

TEST_F(ThreadPoolTest, throws_exception_on_task_error) {
    ThreadPool tp{1};
    auto f = tp.Post([] { throw 0; });
    ASSERT_THROW(f.get(), int);
}

} // namespace Chili

