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

class HelloWorldChannel : public Channel {
    // Use constructors from Channel
    using Channel::Channel;

    // Process incoming requests
    void Process() override {
        auto& res = GetResponse();
        res.SetContent("<h1>Hello world!</h1>");
        res.AppendField("Content-Type", "text/html");
        SendResponse(Status::Ok);
    }
};

class HelloWorldChannelFactory : public ChannelFactory {
    std::shared_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_shared<HelloWorldChannel>(std::move(fs));
    }
};

int main() {
    auto endpoint = IPEndpoint({{127, 0, 0, 1}}, 3000);
    auto factory = std::make_shared<HelloWorldChannelFactory>();

    HttpServer server(endpoint, factory);
    Log::SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Info("HelloWorld Server Started");
    task.wait();
}
