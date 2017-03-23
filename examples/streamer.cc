#include "Channel.h"
#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"

#include <chrono>

using Yam::Http::Channel;
using Yam::Http::ChannelFactory;
using Yam::Http::FileMode;
using Yam::Http::FileStream;
using Yam::Http::HttpServer;
using Yam::Http::IPEndpoint;
using Yam::Http::Log;
using Yam::Http::Status;

using namespace std::literals;

class StreamerChannel : public Channel {
public:
    StreamerChannel(std::shared_ptr<FileStream> fs) :
        Channel(std::move(fs)) {
        ThrottleWrite({1024 * 1024, 1s});
    }

    // Process incoming requests
    Control Process() override {
        try {
            std::string uri(GetRequest().GetUri());
            std::shared_ptr<FileStream> stream = FileStream::Open(uri, FileMode::Read);
            GetResponse().SetContent(stream);
            GetResponse().SetField("Content-Type", "application/octet-stream");
            return SendResponse(Status::Ok);
        } catch (const std::exception& ex) {
            Log::Default()->Error("Error: {}", ex.what());
            GetResponse().Reset();
            return SendFinalResponse(Status::InternalServerError);
        }
    }
};

class StreamerChannelFactory : public ChannelFactory {
    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_unique<StreamerChannel>(std::move(fs));
    }
};

int main() {
    auto endpoint = IPEndpoint({127, 0, 0, 1}, 3000);
    auto factory = std::make_unique<StreamerChannelFactory>();
    auto processingThreads = 1;

    HttpServer server(endpoint, std::move(factory), processingThreads);
    Log::Default()->SetLevel(Log::Level::Info);

    auto task = server.Start();
    Log::Default()->Info("Streamer Server Started");
    task.wait();
}
