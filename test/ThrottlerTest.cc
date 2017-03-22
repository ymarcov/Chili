#include <gmock/gmock.h>

#include "Throttler.h"

#include <chrono>
#include <thread>

using namespace ::testing;
using namespace std::chrono;
using namespace std::literals;

namespace Yam {
namespace Http {

class ThrottlerTest : public Test {
public:
    ThrottlerTest() {
    }

protected:
};

TEST_F(ThrottlerTest, gets_allowed_size_by_elapsed_time) {
    Throttler throttler(0x1000, 100ms);

    auto startTime = steady_clock::now();

    auto startQuota = throttler.GetCurrentQuota();
    EXPECT_EQ(0x1000, startQuota);

    throttler.Consume(0x1000);

    std::this_thread::sleep_for(10ms);
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - startTime);

    EXPECT_LE(throttler.GetCurrentQuota(), 0x1000 * (elapsed.count() / 100.0));

    std::this_thread::sleep_until(throttler.GetFillTime());

    EXPECT_EQ(0x1000, throttler.GetCurrentQuota());
}

TEST_F(ThrottlerTest, gets_desired_quota_fill_time) {
    Throttler throttler(1e9, 1s);

    throttler.Consume(1e9);

    std::this_thread::sleep_until(throttler.GetFillTime(1e9 / 2));
    auto quota = throttler.GetCurrentQuota();

    EXPECT_LT(int(1e9 / 2 - 1e3), quota);
    EXPECT_GT(int(1e9 / 2 + 1e3), quota);
}

} // namespace Http
} // namespace Yam

