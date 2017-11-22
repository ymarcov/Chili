#include <gmock/gmock.h>

#include "PollService.h"
#include "SystemError.h"
#include "WaitEvent.h"

#include <chrono>
#include <fcntl.h>
#include <unistd.h>

using namespace ::testing;
using namespace std::literals;

namespace Yam {
namespace Http {

class PollServiceTest : public Test {
public:
    PollServiceTest() {
    }

protected:
    auto MakePipe() {
        int fds[2];

        if (::pipe(fds))
            throw SystemError();

        return std::make_pair(
            std::make_shared<FileStream>(fds[0]),
            std::make_shared<FileStream>(fds[1]));
    }
};

TEST_F(PollServiceTest, basic_operation) {
    PollService ps(2);
    auto pipe = MakePipe();
    WaitEvent ready;

    auto task = ps.Poll(pipe.first, [&](std::shared_ptr<FileStream> fs, int events) {
        ready.Signal();
    });

    pipe.second->Write("Hello", 6);

    ASSERT_TRUE(ready.Wait(50ms));
    ASSERT_EQ(std::future_status::ready, task.wait_for(50ms));
}

TEST_F(PollServiceTest, negative_case) {
    PollService ps(2);
    auto pipe = MakePipe();
    WaitEvent ready;

    auto task = ps.Poll(pipe.first, [&](std::shared_ptr<FileStream> fs, int events) {
        ready.Signal();
    });

    ASSERT_FALSE(ready.Wait(50ms));
    ASSERT_EQ(std::future_status::timeout, task.wait_for(50ms));
}

} // namespace Http
} // namespace Yam

