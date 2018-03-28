#include "HttpServer.h"
#include "Log.h"
#include "Router.h"

using namespace Chili;

class Application : public Router {
public:
    Application() {
        InstallRoute(Method::Get, "/hello/(.+)", [=](Channel& c, const Args& args) {
            return SayHello(c, args[0]);
        });

        InstallDefault([=](Channel& c, const Args&) {
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
    auto processingThreads = 4;

    HttpServer server(endpoint, factory, processingThreads);
    Log::Default()->SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Default()->Info("Routed Server Started");
    task.wait();
}
