#include "Log.h"
#include "HttpServer.h"
#include "Request.h"
#include "Response.h"
#include "SystemError.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>

using namespace Chili;
using namespace std::literals;

struct ServerConfiguration {
    IPEndpoint _endpoint;
    bool _verbose;
};

std::unique_ptr<HttpServer> CreateServer(ServerConfiguration config, std::unique_ptr<ChannelFactory> channelFactory) {
    return std::make_unique<HttpServer>(config._endpoint, std::move(channelFactory));
}

ServerConfiguration CreateConfiguration(std::vector<std::string> argv) {
    if (argv.size() > 3)
        throw std::runtime_error("Invalid command line arguments");

    auto port = 3000;
    if (argv.size() > 1)
        port = std::stoi(argv[1]);

    auto endpoint = IPEndpoint{{{0, 0, 0, 0}}, port};

    auto verbose = true;

    if (argv.size() >= 3)
        verbose = std::stoi(argv[2]);

    return {endpoint, verbose};
}

void PrintInfo(const Request& request) {
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
    for (auto& name : request.GetHeaderNames()) {
        if (name == "Cookie")
            continue;

        auto header = request.GetHeader(name);
        std::cout << name << ": " << header << "\n";
    }

    /*
     * Print body
     */

    if (request.GetMethod() == Method::Post) {
        std::cout << "====================\n";

        auto str = std::string{request.GetContent().data(), request.GetContent().size()};
        std::cout << str;

        std::cout << "\n====================\n";
    }
}

std::mutex _outputMutex;

std::unique_ptr<ChannelFactory> CreateChannelFactory(const ServerConfiguration& config) {
    static std::shared_ptr<CachedResponse> cr;
    static std::atomic_bool crSet{false};

    struct CustomChannel : Channel {
        CustomChannel(std::shared_ptr<FileStream> fs, bool verbose) :
            Channel(std::move(fs)),
            _verbose(verbose) {}

        void Process() override {
            auto& req = GetRequest();
            auto& res = GetResponse();

            if (_verbose) {
                std::lock_guard<std::mutex> lock(_outputMutex);
                std::cout << "\n";
                PrintInfo(req);
                std::cout << "\n";
            }

            if (crSet) {
                res.UseCached(cr);
                return SendResponse();
            }

            const char msg[] = "<b><u>Hello world!</u></b>";
            auto data = std::make_shared<std::vector<char>>(std::begin(msg), std::end(msg) - 1);
            res.SetContent(data);
            res.SetStatus(Status::Ok);

            cr = res.Cache();
            crSet = true;

            return SendResponse();
        }

        bool _verbose;
    };

    struct CustomChannelFactory : ChannelFactory {
        CustomChannelFactory(bool verbose) :
            _verbose(verbose) {}

        std::shared_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
            return std::make_unique<CustomChannel>(std::move(fs), _verbose);
        }

        bool _verbose;
    };

    return std::make_unique<CustomChannelFactory>(config._verbose);
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
    auto server = CreateServer(config, CreateChannelFactory(config));

    Trap<SIGINT>([&] {
        server->Stop();
    });

    Trap<SIGSEGV>([&] {
        std::cerr << BackTrace{};
    });

    Log::SetLevel(Log::Level::Warning);

    auto serverTask = server->Start();

    try {
        std::cout << "Echo server started.\n";
        serverTask.get();
        std::cout << "\nEcho server exited.\n";
    } catch (const SystemError& e) {
        std::cerr << "\nSYSTEM ERROR: " << e.what() << std::endl;
        std::cerr << e.GetBackTrace() << std::endl;
        std::terminate();

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
    }
}
