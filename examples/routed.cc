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

class Application : public Router {
public:
    Application() {
        InstallRoute(Method::Get, "/hello/(.+)", [=](Channel& c, const Params& params) {
            return SayHello(c, params[0]);
        });

        InstallDefault([=](Channel& c, const Params& params) {
            return PageNotFound(c);
        });
    }

private:
    Status SayHello(Channel& c, const std::string& name) {
        auto html = std::string("<b>Hello, ") + name + "</b>\n";
        c.GetResponse().SetContent(html);
        return Status::Ok;
    }

    Status PageNotFound(Channel& c) {
        auto html = std::string("<h1>404 Not Found</h1>\n");
        c.GetResponse().SetContent(html);
        return Status::NotFound;
    }
};

int main() {
    auto app = std::make_shared<Application>();
    auto endpoint = IPEndpoint({127, 0, 0, 1}, 3000);
    auto factory = std::make_shared<RoutedChannelFactory>(app);
    auto processingThreads = 1;

    HttpServer server(endpoint, factory, processingThreads);
    Log::Default()->SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Default()->Info("Routed Server Started");
    task.wait();
}
