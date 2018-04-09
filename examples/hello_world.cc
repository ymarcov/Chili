#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"

int main() {
    auto endpoint = Chili::IPEndpoint({{127, 0, 0, 1}}, 3000);

    auto factory = Chili::ChannelFactory::Create([](Chili::Channel& c) {
        auto& res = c.GetResponse();
        res.SetContent("<h1>Hello world!</h1>");
        res.AppendHeader("Content-Type", "text/html");
        res.SetStatus(Chili::Status::Ok);
        c.SendResponse();
    });

    Chili::HttpServer server(endpoint, factory);

    auto task = server.Start();
    Chili::Log::Info("HelloWorld Server Started");
    task.wait();
}
