#include <gmock/gmock.h>

#include "ThreadPool.h"

using namespace ::testing;

namespace Chili {

class ThreadPoolTest : public Test {
};

TEST_F(ThreadPoolTest, increments_counter_concurrently) {
    const int threadCount = 10;
    ThreadPool tp{threadCount};
    std::vector<std::future<void>> futures;
    std::atomic_int counter{0};
    const auto target = 200000;

    for (int i = 0; i < target; i++)
        futures.push_back(tp.Post([&] {
            ++counter;
        }));

    for (auto& f : futures)
        f.get();

    ASSERT_EQ(target, counter);
}

TEST_F(ThreadPoolTest, throws_exception_on_task_error) {
    ThreadPool tp{1};
    auto f = tp.Post([] { throw 0; });
    ASSERT_THROW(f.get(), int);
}

} // namespace Chili

