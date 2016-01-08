#include <gmock/gmock.h>

#include "Poller.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "ThreadPool.h"

#include <chrono>
#include <thread>

using namespace ::testing;
using namespace std::literals;

namespace Yam {
namespace Http {

class PollerTest : public Test {
public:
    PollerTest() :
        _poller{MakeThreadPool()},
        _server{MakeEndpoint(), MakeThreadPool()} {
    }

protected:
    std::shared_ptr<TcpConnection> MakeConnection() {
        return std::make_shared<TcpConnection>(_server.GetEndpoint());
    }

    Poller _poller;
    TcpServer _server;

private:
    IPEndpoint MakeEndpoint() {
        return IPEndpoint{{127, 0, 0, 1}, 5555};
    }

    std::shared_ptr<ThreadPool> MakeThreadPool() {
        return std::make_shared<ThreadPool>(1);
    }
};

TEST_F(PollerTest, serves_events) {
    bool gotReadEvent = false;
    bool gotShutdownEvent = false;

    auto pollerTask = _poller.Start([&](std::shared_ptr<FileStream> fs, int events) {
        gotReadEvent |= (events & Poller::Events::Readable);
        gotShutdownEvent |= (events & Poller::Events::Shutdown);
    });

    auto serverTask = _server.Start([&](std::shared_ptr<TcpConnection> conn) {
        _poller.Register(conn);
    });

    MakeConnection();
    std::this_thread::sleep_for(100ms);

    _server.Stop();
    serverTask.get();

    _poller.Stop();
    pollerTask.get();

    EXPECT_TRUE(gotReadEvent);
    EXPECT_TRUE(gotShutdownEvent);
}

} // namespace Http
} // namespace Yam

