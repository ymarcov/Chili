#include <gmock/gmock.h>

#include "HttpServer.h"
#include "Countdown.h"
#include "ExitTrap.h"
#include "FileStream.h"
#include "Log.h"
#include "LogUtils.h"
#include "Poller.h"
#include "TestFileUtils.h"
#include "WaitEvent.h"

#include <atomic>
#include <chrono>
#include <random>

using namespace ::testing;
using namespace std::literals;

namespace Nitra {

const char requestData[] =
"GET /path/to/res HTTP/1.1\r\n"
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
"Accept-encoding: gzip, deflate\r\n"
"Accept-language: en-US,en;q=0.5\r\n"
"Connection: close\r\n"
"Host: request.urih.com\r\n"
"Referer: http://www.google.com/?url=http%3A%2F%2Frequest.urih.com\r\n"
"User-agent: Mozilla/5.0 (X11; Linux x86_64; rv:31.0) Gecko/20100101 Firefox/31.0 Iceweasel/31.8.0\r\n"
"Cookie: Session=abcd1234; User=Yam\r\n"
"X-http-proto: HTTP/1.1\r\n"
"X-log-7527: 95.35.33.46\r\n"
"X-real-ip: 95.35.33.46\r\n"
"Content-Length: 13\r\n"
"\r\n"
"Request body!";

const char requestDataWithExpect[] =
"GET /path/to/res HTTP/1.1\r\n"
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
"Accept-encoding: gzip, deflate\r\n"
"Accept-language: en-US,en;q=0.5\r\n"
"Connection: close\r\n"
"Expect: 100-continue\r\n"
"Host: request.urih.com\r\n"
"Referer: http://www.google.com/?url=http%3A%2F%2Frequest.urih.com\r\n"
"User-agent: Mozilla/5.0 (X11; Linux x86_64; rv:31.0) Gecko/20100101 Firefox/31.0 Iceweasel/31.8.0\r\n"
"Cookie: Session=abcd1234; User=Yam\r\n"
"X-http-proto: HTTP/1.1\r\n"
"X-log-7527: 95.35.33.46\r\n"
"X-real-ip: 95.35.33.46\r\n"
"Content-Length: 13\r\n"
"\r\n";

const char okResponse[] =
"HTTP/1.1 200 OK\r\n"
"Connection: close\r\n"
"Content-Length: 0\r\n"
"\r\n";

const char requestDataWithExpectBody[] = "Request body!";

class OrchestratorTest : public Test {
public:
    OrchestratorTest() {
        Profiler::Enable();
    }

    ~OrchestratorTest() {
        Profiler::Disable();
        Profiler::Clear();
    }

protected:
    void WriteMoreData(std::shared_ptr<FileStream>& fs, int minSize, int maxSize = -1) {
        if (maxSize == -1)
            maxSize = minSize;

        std::random_device rd;
        std::uniform_int_distribution<int> dist(minSize, maxSize);
        auto bufferSize = dist(rd);
        auto buffer = std::vector<char>(bufferSize);

        for (int i = 0; i < bufferSize; i++)
            buffer[i] = dist(rd) % 0xFF;

        fs->Write(buffer.data(), buffer.size());
    }

    using FactoryFunction = std::function<std::unique_ptr<Channel>(std::shared_ptr<FileStream>)>;

    std::shared_ptr<HttpServer> MakeServer(FactoryFunction factoryFunction) {
        struct Factory : ChannelFactory {
            Factory(FactoryFunction f) :
                _f(std::move(f)) {}

            std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> stream) override {
                return _f(std::move(stream));
            }

            FactoryFunction _f;
        };

        return std::make_shared<HttpServer>(_ep, std::make_unique<Factory>(std::move(factoryFunction)), 4);
    }

    template <class Processor>
    FactoryFunction MakeProcessor(Processor processor) {
        struct CustomChannel : Channel {
            CustomChannel(std::shared_ptr<FileStream> fs, Processor p) :
                Channel(std::move(fs)),
                _p(std::move(p)) {
                SetAutoFetchContent(false);
            }

            Control Process(const Request&, Response&) override {
                return _p(*this);
            }

            Processor _p;
        };

        return [=](std::shared_ptr<FileStream> fs) {
            return std::make_unique<CustomChannel>(std::move(fs), std::move(processor));
        };
    }

    std::unique_ptr<TcpConnection> CreateClient() {
        return std::make_unique<TcpConnection>(_ep);
    }

    std::string ReadToEnd(FileStream& conn) {
        conn.SetBlocking(true);
        std::string result;
        char buffer[1];

        while (auto bytesRead = conn.Read(buffer, sizeof(buffer), 1s))
            result += {buffer, bytesRead};

        conn.SetBlocking(false);

        return result;
    }

    std::string ReadAvailable(FileStream& conn) {
        char buffer[0x1000];
        auto bytesRead = conn.Read(buffer, sizeof(buffer));
        return {buffer, bytesRead};
    }

    IPEndpoint _ep{{{127, 0, 0, 1}}, 63184};
};

TEST_F(OrchestratorTest, one_client_header_only) {
    auto ready = std::make_shared<WaitEvent>();

    auto server = MakeServer(MakeProcessor([=](Channel& c) {
        if (c.GetRequest().GetField("Host") == "request.urih.com")
            ready->Signal();

        return c.SendResponse(Status::Ok);
    }));

    server->Start();

    auto client = CreateClient();

    client->Write(requestData, sizeof(requestData));
    ASSERT_TRUE(ready->Wait(200ms));

    std::string response;
    EXPECT_NO_THROW(response = ReadToEnd(*client));
    EXPECT_EQ(okResponse, response);
}

TEST_F(OrchestratorTest, one_client_header_and_body) {
    auto ready = std::make_shared<WaitEvent>();

    auto server = MakeServer(MakeProcessor([=](Channel& c) {
        if (!c.GetRequest().IsContentAvailable())
            return c.FetchContent();

        ready->Signal();
        return c.SendResponse(Status::Ok);
    }));

    server->Start();

    auto client = CreateClient();

    client->Write(requestData, sizeof(requestData));
    ASSERT_TRUE(ready->Wait(200ms));

    std::string response;
    EXPECT_NO_THROW(response = ReadToEnd(*client));
    EXPECT_EQ(okResponse, response);
}

TEST_F(OrchestratorTest, one_client_header_and_body_throttled) {
    auto ready = std::make_shared<WaitEvent>();

    auto server = MakeServer(MakeProcessor([=](Channel& c) {
        if (!c.IsWriteThrottled())
            c.ThrottleWrite({5, 5ms});

        if (!c.IsReadThrottled())
            c.ThrottleRead({5, 5ms});

        if (!c.GetRequest().IsContentAvailable())
            return c.FetchContent();

        ready->Signal();
        return c.SendResponse(Status::Ok);
    }));

    server->Start();

    auto client = CreateClient();

    client->Write(requestData, sizeof(requestData));
    ASSERT_TRUE(ready->Wait(200ms));

    std::string response;
    EXPECT_NO_THROW(response = ReadToEnd(*client));
    EXPECT_EQ(okResponse, response);
}

TEST_F(OrchestratorTest, one_client_header_and_body_with_expect) {
    auto sentContinue = std::make_shared<WaitEvent>();
    auto sentOk = std::make_shared<WaitEvent>();

    auto server = MakeServer(MakeProcessor([=](Channel& c) {
        if (!c.GetRequest().IsContentAvailable()) {
            sentContinue->Signal();
            return c.FetchContent();
        }

        sentOk->Signal();
        return c.SendResponse(Status::Ok);
    }));

    server->Start();

    auto client = CreateClient();

    std::string response;

    client->Write(requestDataWithExpect, sizeof(requestDataWithExpect));
    ASSERT_TRUE(sentContinue->Wait(200ms));
    std::this_thread::sleep_for(50ms);
    response = ReadAvailable(*client);
    ASSERT_EQ("HTTP/1.1 100 Continue\r\nContent-Length: 0\r\n\r\n", response);

    client->Write(requestDataWithExpectBody, sizeof(requestDataWithExpectBody));

    ASSERT_TRUE(sentOk->Wait(200ms));
    std::this_thread::sleep_for(50ms);
    response = ReadAvailable(*client);
    ASSERT_EQ(okResponse, response);
}

TEST_F(OrchestratorTest, one_client_header_and_body_with_expect_reject) {
    auto sentRejection = std::make_shared<WaitEvent>();

    auto server = MakeServer(MakeProcessor([=](Channel& c) {
        sentRejection->Signal();
        return c.RejectContent();
    }));

    server->Start();

    auto client = CreateClient();

    client->Write(requestDataWithExpect, sizeof(requestDataWithExpect));
    ASSERT_TRUE(sentRejection->Wait(200ms));
    std::this_thread::sleep_for(50ms);

    std::string response;
    ASSERT_NO_THROW(response = ReadAvailable(*client));
    ASSERT_EQ("HTTP/1.1 417 Expectation Failed\r\nContent-Length: 0\r\n\r\n", response);
}

TEST_F(OrchestratorTest, stress_sync) {
    auto ready = std::make_shared<WaitEvent>();
    auto readyCount = std::make_shared<std::atomic_int>(0);
    const auto clientCount = 5000;
    auto lightLog = TemporaryLogLevel(Log::Level::Warning);

    auto server = MakeServer(MakeProcessor([=](Channel& c) {
        if (!c.IsWriteThrottled())
            c.ThrottleWrite({5, 5ms});

        if (!c.GetRequest().IsContentAvailable())
            return c.FetchContent();

        if (c.GetRequest().GetField("Host") == "request.urih.com")
            if (++*readyCount == clientCount)
                ready->Signal();

        return c.SendResponse(Status::Ok);
    }));

    server->Start();
    auto clients = std::vector<std::shared_ptr<TcpConnection>>();

    for (auto i = 0; i < clientCount; ++i)
        clients.push_back(CreateClient());

    std::random_device rd;
    std::shuffle(begin(clients), end(clients), rd);

    Poller poller(1);
    std::atomic_int writtenCount{0};
    WaitEvent allWritten;

    poller.Start([&](std::shared_ptr<FileStream> fs, int events) {
        if (ReadAvailable(*fs) == okResponse)
            if (++writtenCount == clientCount)
                allWritten.Signal();
    });

    for (auto& client : clients) {
        client->Write(requestData, sizeof(requestData));
        poller.Poll(client, Poller::Events::EndOfStream);
    }

    ASSERT_TRUE(ready->Wait(10s));
    ASSERT_TRUE(allWritten.Wait(10s));
}

} // namespace Nitra


