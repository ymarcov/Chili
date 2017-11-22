#include <gmock/gmock.h>

#include "Countdown.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace ::testing;
using namespace std::literals;

namespace Yam {
namespace Http {

class CountdownTest : public Test {
public:
    CountdownTest() {
    }

protected:
};

TEST_F(CountdownTest, basic_operation) {
    Countdown c(10);
    std::atomic_int count(0);

    auto task = std::async([&] {
        while (c.Decrement()) {
            ++count;
            std::this_thread::sleep_for(10ms);
        }
    });

    c.Wait();
    ASSERT_EQ(c.GetInitialValue() - 1, count);
}

} // namespace Http
} // namespace Yam

