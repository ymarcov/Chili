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
        _poller{std::make_shared<Poller>(MakeThreadPool())},
        _server{MakeEndpoint(), _poller} {
    }

protected:
    std::shared_ptr<TcpConnection> MakeConnection() {
        return std::make_shared<TcpConnection>(_server.GetEndpoint());
    }

    std::shared_ptr<Poller> _poller;
    TcpServer _server;

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

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream>, int events) {
        gotShutdownEvent |= (events & Poller::Events::Shutdown);
        return Poller::Registration::Continue;
    });

    auto serverTask = _server.Start();

    MakeConnection();
    std::this_thread::sleep_for(100ms);

    _server.Stop();
    serverTask.get();

    _poller->Stop();
    pollerTask.get();

    EXPECT_TRUE(gotShutdownEvent);
}

TEST_F(PollerTest, signals_read_events) {
    unsigned countedReadEvents = 0;

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream>, int events) {
        if (!(events & Poller::Events::Shutdown))
            countedReadEvents += !!(events & Poller::Events::Readable);
        return Poller::Registration::Continue;
    });

    auto serverTask = _server.Start();

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

    _poller->Stop();
    pollerTask.get();

    EXPECT_EQ(3, countedReadEvents);
}

TEST_F(PollerTest, reaps_connections) {
    auto pollerTask = _poller->Start([](std::shared_ptr<FileStream>, int) {
        return Poller::Registration::Conclude;
    });

    auto serverTask = _server.Start();

    EXPECT_EQ(0, _poller->GetWatchedCount());

    {
        auto c1 = MakeConnection();
        auto c2 = MakeConnection();
        auto c3 = MakeConnection();

        std::this_thread::sleep_for(50ms);
        EXPECT_EQ(3, _poller->GetWatchedCount());
    }

    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(0, _poller->GetWatchedCount());

    _server.Stop();
    serverTask.get();

    _poller->Stop();
    pollerTask.get();
}

TEST_F(PollerTest, notifies_on_stop) {
    auto pollerTask = _poller->Start([](std::shared_ptr<FileStream>, int) {
        return Poller::Registration::Conclude;
    });

    bool notified = false;

    _poller->OnStop += [&] { notified = true; };

    _poller->Stop();

    EXPECT_EQ(std::future_status::ready, pollerTask.wait_for(100ms));
    EXPECT_TRUE(notified);
}

} // namespace Http
} // namespace Yam

