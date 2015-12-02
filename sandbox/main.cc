#include "TcpServer.h"

#include <iostream>
#include <string>

using namespace Yam::Http;

int main(int argc, char* argv[]) {
    int threadCount = 1;

    if (argc > 1)
        threadCount = std::stoi(argv[1]);

    std::cout << "Sandbox started.\n";

    auto threadPool = std::make_shared<ThreadPool>(threadCount);
    auto endpoint = IPEndpoint{{127, 0,0, 1}, 3000};

    TcpServer server{endpoint, threadPool};

    auto handler = [](std::shared_ptr<TcpConnection> conn) {
        std::cout << "Connection received!\n";
        conn->Write("hello!\n", 8);
    };

    server.Start(handler).get();
}
