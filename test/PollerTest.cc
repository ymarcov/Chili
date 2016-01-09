#include <gmock/gmock.h>

#include "Poller.h"
#include "TcpConnection.h"
#include "ThreadedTcpServer.h"
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
    ThreadedTcpServer _server;

private:
    IPEndpoint MakeEndpoint() {
        return IPEndpoint{{127, 0, 0, 1}, 5555};
    }

    std::shared_ptr<ThreadPool> MakeThreadPool() {
        return std::make_shared<ThreadPool>(1);
    }
};

TEST_F(PollerTest, signals_shutdown_event) {
    bool gotShutdownEvent = false;

    auto pollerTask = _poller.Start([&](std::shared_ptr<FileStream>, int events) {
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

    EXPECT_TRUE(gotShutdownEvent);
}

TEST_F(PollerTest, signals_read_events) {
    unsigned countedReadEvents = 0;

    auto pollerTask = _poller.Start([&](std::shared_ptr<FileStream>, int events) {
        if (!(events & Poller::Events::Shutdown))
            countedReadEvents += !!(events & Poller::Events::Readable);
    });

    auto serverTask = _server.Start([&](std::shared_ptr<TcpConnection> conn) {
        _poller.Register(conn);
    });

    {
        auto c1 = MakeConnection();
        c1->Write("hello", 5);
        auto c2 = MakeConnection();
        c2->Write("hello", 5);
        auto c3 = MakeConnection();
        c3->Write("hello", 5);

        std::this_thread::sleep_for(100ms);
    }

    _server.Stop();
    serverTask.get();

    _poller.Stop();
    pollerTask.get();

    EXPECT_EQ(3, countedReadEvents);
}

} // namespace Http
} // namespace Yam

