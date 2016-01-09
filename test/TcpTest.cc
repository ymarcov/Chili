#include <gmock/gmock.h>

#include "TcpConnection.h"
#include "ThreadedTcpServer.h"

using namespace ::testing;

namespace Yam {
namespace Http {

struct ThreadCount {
    ThreadCount(int n = 1) : _n(n) {}
    int _n;
};

ThreadCount operator "" _threads(unsigned long long n) {
    return ThreadCount{static_cast<int>(n)};
}

class TcpTest : public Test {
protected:
    std::unique_ptr<ThreadedTcpServer> CreateServer(ThreadCount tc = ThreadCount{}) {
        return std::make_unique<ThreadedTcpServer>(_testEp, std::make_shared<ThreadPool>(tc._n));
    }

    std::unique_ptr<TcpConnection> CreateClient() {
        return std::make_unique<TcpConnection>(_testEp);
    }

    ThreadedTcpServer::ConnectionHandler SayGoodbyeAndClose() {
        return [](std::shared_ptr<TcpConnection> conn) {
            char buffer[0x100] = {0};
            auto n = conn->Read(buffer, sizeof(buffer));
            EXPECT_EQ("hello", std::string(buffer, n));
            conn->Write("goodbye", 7);
        };
    }

    void SayHello(TcpConnection& c) {
        c.Write("hello", 5);
    }

    void WaitForGoodbye(TcpConnection& c) {
        char buffer[0x100];
        auto n = c.Read(buffer, sizeof(buffer));
        EXPECT_EQ(7, n);
    }

private:
    IPEndpoint _testEp{{127, 0, 0, 1}, 8888};
};

TEST_F(TcpTest, hello_goodbye) {
    auto server = CreateServer();
    auto task = server->Start(SayGoodbyeAndClose());

    auto client = CreateClient();
    SayHello(*client);
    WaitForGoodbye(*client);

    server->Stop();
    task.get();
}

TEST_F(TcpTest, multiple_clients) {
    auto server = CreateServer(50_threads);
    auto task = server->Start(SayGoodbyeAndClose());

    std::vector<std::unique_ptr<TcpConnection>> clients(100);
    for (auto& c : clients)
        c = CreateClient();
    for (auto& c : clients) {
        SayHello(*c);
        WaitForGoodbye(*c);
    }

    server->Stop();
    task.get();
}

TEST_F(TcpTest, server_restart) {
    auto server = CreateServer();
    auto task = server->Start(SayGoodbyeAndClose());
    server->Stop();
    task.get();
    task = server->Start(SayGoodbyeAndClose());

    auto c = CreateClient();
    SayHello(*c);
    WaitForGoodbye(*c);

    server->Stop();
    task.get();
}

} // namespace Http
} // namespace Yam

