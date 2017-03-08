#include <gmock/gmock.h>

#include "Poller.h"
#include "TcpConnection.h"
#include "PolledTcpServer.h"
#include "ThreadPool.h"
#include "WaitEvent.h"

#include <chrono>
#include <condition_variable>
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
    PolledTcpServer _server;

private:
    IPEndpoint MakeEndpoint() {
        return IPEndpoint{{{127, 0, 0, 1}}, 51788};
    }

    std::shared_ptr<ThreadPool> MakeThreadPool() {
        return std::make_shared<ThreadPool>(3);
    }
};

TEST_F(PollerTest, signals_end_of_stream_event) {
    bool gotEosEvent = false;

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream>, int events) {
        gotEosEvent |= (events & Poller::Events::EndOfStream);
    });

    auto serverTask = _server.Start();

    MakeConnection();
    std::this_thread::sleep_for(100ms);

    _server.Stop();
    serverTask.get();

    _poller->Stop();
    pollerTask.get();

    EXPECT_TRUE(gotEosEvent);
}

//FIXME: sometimes blocks
TEST_F(PollerTest, registered_file_does_not_block) {
    WaitEvent firstPartDone, secondPartDone;
    std::atomic_size_t totalBytesRemaining{10};

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream> f, int events) {
        char buffer[1];

        while (auto bytesRead = f->Read(buffer, sizeof(buffer)))
            totalBytesRemaining -= bytesRead;

        firstPartDone.Signal();

        if (totalBytesRemaining > 0) {
            _poller->Poll(f);
            return;
        } else {
            secondPartDone.Signal();
            return;
        }
    });

    auto serverTask = _server.Start();

    {
        auto conn = MakeConnection();
        conn->Write("hello", 5);
        firstPartDone.Wait();
        std::this_thread::sleep_for(20ms);
        conn->Write("world", 5);
        secondPartDone.Wait();
    }

    _server.Stop();
    serverTask.get();

    _poller->Stop();
    pollerTask.get();

    EXPECT_EQ(0, totalBytesRemaining);
}

//FIXME: sometimes 3 == 4
TEST_F(PollerTest, signals_read_events) {
    std::atomic_int countedReadEvents{0};

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream> fs, int events) {
        countedReadEvents += !!(events & Poller::Events::Readable);
    });

    auto serverTask = _server.Start();

    {
        auto c1 = MakeConnection();
        auto c2 = MakeConnection();
        auto c3 = MakeConnection();

        c1->Write("hello", 5);
        c2->Write("hello", 5);
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
    std::atomic_int processingCount{0};
    WaitEvent allProcessing, mayStopProcessing;

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream>, int events) {
        if (++processingCount == 3)
            allProcessing.Signal();
        mayStopProcessing.Wait();
    });

    auto serverTask = _server.Start();

    EXPECT_EQ(0, _poller->GetWatchedCount());

    {
        auto c1 = MakeConnection();
        auto c2 = MakeConnection();
        auto c3 = MakeConnection();

        c1->Write("hello", 5);
        c2->Write("hello", 5);
        c3->Write("hello", 5);

        allProcessing.Wait();
        EXPECT_EQ(3, _poller->GetWatchedCount());
        mayStopProcessing.Signal();
    }

    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(0, _poller->GetWatchedCount());

    _server.Stop();
    serverTask.get();

    _poller->Stop();
    pollerTask.get();
}

TEST_F(PollerTest, reports_disconnect) {
    WaitEvent ready, reported;
    std::atomic_bool reportedDisconnect{false};

    auto pollerTask = _poller->Start([&](std::shared_ptr<FileStream> fs, int events) {
        ready.Signal();

        if (events & Poller::Events::EndOfStream) {
            reportedDisconnect = true;
            reported.Signal();
        } else {
            // events weren't combined, need to re-wait for EOS
            _poller->Poll(fs);
        }
    });

    auto serverTask = _server.Start();

    {
        auto conn = MakeConnection();
        conn->Write("hello", 5);
        ready.Wait();
    }

    reported.Wait(20ms);

    _server.Stop();
    serverTask.get();

    _poller->Stop();
    pollerTask.get();

    EXPECT_TRUE(reportedDisconnect);
}

TEST_F(PollerTest, notifies_on_stop) {
    auto pollerTask = _poller->Start([](std::shared_ptr<FileStream>, int) {
    });

    bool notified = false;

    _poller->OnStop += [&] { notified = true; };

    _poller->Stop();

    EXPECT_EQ(std::future_status::ready, pollerTask.wait_for(100ms));
    EXPECT_TRUE(notified);
}

} // namespace Http
} // namespace Yam

