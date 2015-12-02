#include "MemoryPool.h"
#include "Request.h"
#include "TcpServer.h"

#include <iostream>
#include <memory>
#include <string>

using namespace Yam::Http;

struct ServerConfiguration {
    IPEndpoint _endpoint;
    std::shared_ptr<ThreadPool> _threadPool;
};

std::unique_ptr<TcpServer> CreateServer(ServerConfiguration config) {
    return std::make_unique<TcpServer>(config._endpoint, config._threadPool);
}

ServerConfiguration CreateConfiguration(std::vector<std::string> argv) {
    if (argv.size() > 2)
        throw std::runtime_error("Invalid command line arguments");

    auto port = 3000;
    if (argv.size() > 1)
        port = std::stoi(argv[1]);

    auto endpoint = IPEndpoint{{127, 0, 0, 1}, port};

    int threadCount = 1;
    if (argv.size() == 3)
        threadCount = std::stoi(argv[2]);

    auto threadPool = std::make_shared<ThreadPool>(threadCount);

    return {endpoint, threadPool};
}

void PrintInfo(const Request& request) {
    /*
     * Print request line
     */

    switch (request.GetMethod()) {
        case Protocol::Method::Get:
            std::cout << "GET ";
            break;
        case Protocol::Method::Head:
            std::cout << "HEAD ";
            break;
        case Protocol::Method::Post:
            std::cout << "POST ";
            break;
    }

    std::cout << request.GetUri() << " ";

    switch (request.GetProtocol()) {
        case Protocol::Version::Http10:
            std::cout << "HTTP/1.0\n";
            break;
        case Protocol::Version::Http11:
            std::cout << "HTTP/1.1\n";
            break;
    }

    /*
     * Print cookies
     */

    for (auto& cookieName : request.GetCookieNames()) {
        auto cookie = request.GetCookie(cookieName);
        std::cout << "Cookie: " << cookieName << " = " << cookie << "\n";
    }
}

TcpServer::ConnectionHandler CreateHandler() {
    auto memoryPool = MemoryPool<Request::Buffer>::Create();

    return [=](std::shared_ptr<TcpConnection> conn) {
        std::cout << "Connection received!\n";

        auto buffer = memoryPool->New();
        conn->Read(buffer.get(), sizeof(Request::Buffer));
        auto request = Request{std::move(buffer)};

        PrintInfo(request);
        conn->Write("hello!\n", 8);
    };
}

std::vector<std::string> ArgsToVector(int argc, char* argv[]) {
    return std::vector<std::string>{argv, argv + argc};
}

int main(int argc, char* argv[]) {
    auto args = ArgsToVector(argc, argv);
    auto config = CreateConfiguration(args);
    auto server = CreateServer(config);

    auto task = server->Start(CreateHandler());
    std::cout << "Sandbox started.\n";
    task.get();
}
