#include "Channel.h"
#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"

using Yam::Http::Channel;
using Yam::Http::ChannelFactory;
using Yam::Http::FileStream;
using Yam::Http::HttpServer;
using Yam::Http::IPEndpoint;
using Yam::Http::Log;
using Yam::Http::Status;

class HelloWorldChannel : public Channel {
    // Use constructors from Channel
    using Channel::Channel;

    // Process incoming requests
    Control Process() override {
        GetResponse().SetContent(CreateHtml());
        GetResponse().SetField("Content-Type", "text/html");
        return SendResponse(Status::Ok);
    }

    std::shared_ptr<std::vector<char>> CreateHtml() const {
        auto text = std::string("<u><b>Hello world!</b></u>\n");
        return std::make_shared<std::vector<char>>(begin(text), end(text));
    }
};

class HelloWorldChannelFactory : public ChannelFactory {
    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_unique<HelloWorldChannel>(std::move(fs));
    }
};

int main() {
    auto endpoint = IPEndpoint({127, 0, 0, 1}, 3000);
    auto factory = std::make_unique<HelloWorldChannelFactory>();
    auto processingThreads = 1;

    HttpServer server(endpoint, std::move(factory), processingThreads);
    Log::Default()->SetLevel(Log::Level::Info);
    auto task = server.Start();
    Log::Default()->Info("HelloWorld Server Started");
    task.wait();
}
