#include "Channel.h"
#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"

using Chili::Channel;
using Chili::ChannelFactory;
using Chili::FileStream;
using Chili::HttpServer;
using Chili::IPEndpoint;
using Chili::Log;
using Chili::Request;
using Chili::Response;
using Chili::Status;

int main() {
    auto endpoint = IPEndpoint({{127, 0, 0, 1}}, 3000);

    auto factory = ChannelFactory::Create([](Channel& c) {
        auto& res = c.GetResponse();
        res.SetContent("<h1>Hello world!</h1>");
        res.AppendField("Content-Type", "text/html");
        c.SendResponse(Status::Ok);
    });

    HttpServer server(endpoint, factory);
    Log::SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Info("HelloWorld Server Started");
    task.wait();
}
