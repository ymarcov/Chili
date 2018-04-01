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
    Control Process(const Request&, Response& res) override {
        res.SetContent("<h1>Hello world!</h1>");
        res.AppendField("Content-Type", "text/html");
        return SendResponse(Status::Ok);
    }
};

class HelloWorldChannelFactory : public ChannelFactory {
    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_unique<HelloWorldChannel>(std::move(fs));
    }
};

int main() {
    auto endpoint = IPEndpoint({{127, 0, 0, 1}}, 3000);
    auto factory = std::make_shared<HelloWorldChannelFactory>();
    auto processingThreads = 1;

    HttpServer server(endpoint, factory, processingThreads);
    Log::SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Info("HelloWorld Server Started");
    task.wait();
}
