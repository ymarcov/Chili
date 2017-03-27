#include "Channel.h"
#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"
#include "Router.h"

using namespace Yam::Http;

class RoutedChannel : public Channel {
public:
    RoutedChannel(std::shared_ptr<FileStream> fs, std::shared_ptr<Router> router) :
        Channel(std::move(fs)),
        _router(std::move(router)) {}

    // Process incoming requests
    Control Process(const Request&, Response&) override {
        return _router->InvokeRoute(*this);
    }

private:
    std::shared_ptr<Router> _router;
};

class RoutedChannelFactory : public ChannelFactory {
public:
    RoutedChannelFactory(std::shared_ptr<Router> router) :
        _router(std::move(router)) {}

    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_unique<RoutedChannel>(std::move(fs), _router);
    }

private:
    std::shared_ptr<Router> _router;
};

std::shared_ptr<std::vector<char>> CreateGreetingPage(const std::string& name) {
    auto html = std::string("<b>Hello, ") + name + "</b>\n";

    return std::make_shared<std::vector<char>>(
        html.data(),
        html.data() + html.size()
    );
}

std::shared_ptr<std::vector<char>> Create404Page() {
    auto html = std::string("<h1>404 Not Found</h1>\n");

    return std::make_shared<std::vector<char>>(
        html.data(),
        html.data() + html.size()
    );
}

std::shared_ptr<Router> CreateRouter() {
    auto router = std::make_shared<Router>();

    router->InstallRoute(Method::Get, "/hello/(.+)", [](auto& channel, auto& params) {
        channel.GetResponse().SetContent(CreateGreetingPage(params[0]));
        return Status::Ok;
    });

    router->InstallDefault([](auto& channel, auto& params) {
        channel.GetResponse().SetContent(Create404Page());
        return Status::NotFound;
    });

    return router;
}

int main() {
    auto endpoint = IPEndpoint({127, 0, 0, 1}, 3000);
    auto factory = std::make_unique<RoutedChannelFactory>(CreateRouter());
    auto processingThreads = 1;

    HttpServer server(endpoint, std::move(factory), processingThreads);
    Log::Default()->SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Default()->Info("Routed Server Started");
    task.wait();
}
