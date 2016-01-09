#include "MemoryPool.h"
#include "Request.h"
#include "Responder.h"
#include "TcpServer.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>

using namespace Yam::Http;
using namespace std::literals;

struct ServerConfiguration {
    IPEndpoint _endpoint;
    std::shared_ptr<ThreadPool> _threadPool;
    std::shared_ptr<Poller> _poller;
};

std::unique_ptr<TcpServer> CreateServer(ServerConfiguration config) {
    return std::make_unique<TcpServer>(config._endpoint, config._poller);
}

ServerConfiguration CreateConfiguration(std::vector<std::string> argv) {
    if (argv.size() > 3)
        throw std::runtime_error("Invalid command line arguments");

    auto port = 3000;
    if (argv.size() > 1)
        port = std::stoi(argv[1]);

    auto endpoint = IPEndpoint{{0, 0, 0, 0}, port};

    int threadCount = 1;
    if (argv.size() == 3)
        threadCount = std::stoi(argv[2]);

    auto threadPool = std::make_shared<ThreadPool>(threadCount);

    return {endpoint, threadPool, std::make_shared<Poller>(threadPool)};
}

void PrintInfo(Request& request) {
    /*
     * Print request line
     */

    switch (request.GetMethod()) {
        case Method::Get:
            std::cout << "GET ";
            break;
        case Method::Head:
            std::cout << "HEAD ";
            break;
        case Method::Post:
            std::cout << "POST ";
            break;
        default:
            std::cout << "Unsupported ";
            break;
    }

    std::cout << request.GetUri() << " ";

    switch (request.GetVersion()) {
        case Version::Http10:
            std::cout << "HTTP/1.0\n";
            break;
        case Version::Http11:
            std::cout << "HTTP/1.1\n";
            break;
    }

    /*
     * Print various header fields
     */
    for (auto& fieldName : request.GetFieldNames()) {
        if (fieldName == "Cookie")
            continue;

        auto field = request.GetField(fieldName);
        std::cout << fieldName << ": " << field << "\n";
    }

    /*
     * Print cookies
     */

    for (auto& cookieName : request.GetCookieNames()) {
        auto cookie = request.GetCookie(cookieName);
        std::cout << "Cookie: " << cookieName << " = " << cookie << "\n";
    }

    /*
     * Print body
     */

    if (request.GetMethod() == Method::Post) {
        std::cout << "====================\n";
        auto remaining = request.GetContentLength();
        while (remaining) {
            char buffer[0x100];
            auto bytesRead = request.ReadNextBodyChunk(buffer, sizeof(buffer));
            auto str = std::string{buffer, bytesRead};
            std::cout << str;
            remaining -= bytesRead;
        }
        std::cout << "====================\n";
    }
}

std::mutex _outputMutex;

Poller::EventHandler CreateHandler(ServerConfiguration config) {
    auto totalPages = 2 * config._threadPool->GetThreadCount();
    auto memoryPool = MemoryPool<Request::Buffer>::Create(totalPages);

    return [=](std::shared_ptr<FileStream> conn, int events) {
        try {
            auto request = Request{memoryPool->New(), conn};
            auto responder = Responder{conn};

            if (request.GetField("Expect", nullptr))
                responder.Send(Status::Continue);

            {
                std::lock_guard<std::mutex> lock{_outputMutex};
                std::cout << "\n";
                PrintInfo(request);
                std::cout << "\n";
            }

            responder.Send(Status::Ok);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock{_outputMutex};
            std::cout << "E: " << e.what() << "\n";
        }

        return Poller::Registration::Conclude;
    };
}

std::vector<std::string> ArgsToVector(int argc, char* argv[]) {
    return std::vector<std::string>{argv, argv + argc};
}

void TrapInterrupt(std::function<void()> handler) {
    static std::function<void()> signalHandler;

    signalHandler = handler;
    struct ::sigaction action;
    action.sa_handler = [](int) { signalHandler(); };
    ::sigaction(SIGINT, &action, nullptr);
}

int main(int argc, char* argv[]) {
    auto args = ArgsToVector(argc, argv);

    auto config = CreateConfiguration(args);
    auto server = CreateServer(config);
    auto handler = CreateHandler(config);

    TrapInterrupt([&] { server->Stop(); });

    auto pTask = config._poller->Start(handler);
    auto task = server->Start();
    std::cout << "Echo server started.\n";
    task.get();
    std::cout << "\nEcho server exited.\n";
}
