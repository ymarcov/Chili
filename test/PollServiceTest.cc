#include <gmock/gmock.h>

#include "PollService.h"
#include "Pipe.h"
#include "SystemError.h"
#include "WaitEvent.h"

#include <chrono>

using namespace ::testing;
using namespace std::literals;

namespace Yam {
namespace Http {

class PollServiceTest : public Test {
};

TEST_F(PollServiceTest, basic_operation) {
    PollService ps(2);
    auto pipe = Pipe::Create();
    WaitEvent ready;

    auto task = ps.Poll(pipe.Read, [&](std::shared_ptr<FileStream> fs, int events) {
        ready.Signal();
    });

    pipe.Write->Write("Hello", 6);

    ASSERT_TRUE(ready.Wait(50ms));
    ASSERT_EQ(std::future_status::ready, task.wait_for(50ms));
}

TEST_F(PollServiceTest, negative_case) {
    PollService ps(2);
    auto pipe = Pipe::Create();
    WaitEvent ready;

    auto task = ps.Poll(pipe.Read, [&](std::shared_ptr<FileStream> fs, int events) {
        ready.Signal();
    });

    ASSERT_FALSE(ready.Wait(50ms));
    ASSERT_EQ(std::future_status::timeout, task.wait_for(50ms));
}

} // namespace Http
} // namespace Yam

