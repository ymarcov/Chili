#include "MemoryPool.h"
#include "Request.h"
#include "Responder.h"
#include "SystemError.h"
#include "PolledTcpServer.h"

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
    std::size_t _totalPoolPages;
    std::shared_ptr<Poller> _poller;
    bool _verbose;
};

std::unique_ptr<PolledTcpServer> CreateServer(ServerConfiguration config) {
    return std::make_unique<PolledTcpServer>(config._endpoint, config._poller);
}

ServerConfiguration CreateConfiguration(std::vector<std::string> argv) {
    if (argv.size() > 4)
        throw std::runtime_error("Invalid command line arguments");

    auto port = 3000;
    if (argv.size() > 1)
        port = std::stoi(argv[1]);

    auto endpoint = IPEndpoint{{{0, 0, 0, 0}}, port};

    int threadCount = 1;
    if (argv.size() >= 3)
        threadCount = std::stoi(argv[2]);

    auto threadPool = std::make_shared<ThreadPool>(threadCount);
    auto totalPoolPages = (sizeof(Request::Buffer) / ::getpagesize()) * threadCount;

    auto verbose = true;

    if (argv.size() >= 4)
        verbose = std::stoi(argv[3]);

    return {endpoint, threadPool, totalPoolPages, std::make_shared<Poller>(threadPool), verbose};
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
    auto memoryPool = MemoryPool<Request::Buffer>::Create(config._totalPoolPages);
    auto verbose = config._verbose;

    return [=](std::shared_ptr<FileStream> conn, int events) {
        try {
            auto request = Request{memoryPool->New(), conn};
            auto responder = Responder{conn};

            if (request.GetField("Expect", nullptr))
                responder.Send(Status::Continue);

            if (verbose) {
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

template <int Signal>
void Trap(std::function<void()> handler) {
    static std::function<void()> signalHandler;

    signalHandler = handler;
    struct ::sigaction action;
    action.sa_handler = [](int) { signalHandler(); };
    ::sigaction(Signal, &action, nullptr);
}

int main(int argc, char* argv[]) {
    auto args = ArgsToVector(argc, argv);

    auto config = CreateConfiguration(args);
    auto server = CreateServer(config);
    auto handler = CreateHandler(config);

    Trap<SIGINT>([&] {
        server->Stop();
        config._poller->Stop();
    });

    Trap<SIGSEGV>([&] {
        std::cerr << BackTrace{};
    });

    auto pollerTask = config._poller->Start(handler);
    auto serverTask = server->Start();

    try {
        std::cout << "Echo server started.\n";
        serverTask.get();
        pollerTask.get();
        std::cout << "\nEcho server exited.\n";
    } catch (const SystemError& e) {
        std::cerr << "\nSYSTEM ERROR: " << e.what() << std::endl;
        std::cerr << e.GetBackTrace() << std::endl;
        std::terminate();

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
    }
}
