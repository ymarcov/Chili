#include "Channel.h"
#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"

#include <chrono>

using Chili::Channel;
using Chili::ChannelFactory;
using Chili::FileMode;
using Chili::FileStream;
using Chili::HttpServer;
using Chili::IPEndpoint;
using Chili::Log;
using Chili::Request;
using Chili::Response;
using Chili::Status;

using namespace std::literals;

class StreamerChannel : public Channel {
public:
    StreamerChannel(std::shared_ptr<FileStream> fs) :
        Channel(std::move(fs)) {
        ThrottleWrite({1024 * 1024, 1s});
    }

    // Process incoming requests
    void Process() override {
        auto& req = GetRequest();
        auto& res = GetResponse();

        try {
            std::string uri(req.GetUri());
            std::shared_ptr<FileStream> stream = FileStream::Open(uri, FileMode::Read);
            res.SetContent(stream);
            res.AppendField("Content-Type", "application/octet-stream");
            return SendResponse(Status::Ok);
        } catch (const std::exception& ex) {
            Log::Error("Error: {}", ex.what());
            res.Reset();
            return SendFinalResponse(Status::InternalServerError);
        }
    }
};

class StreamerChannelFactory : public ChannelFactory {
    std::shared_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override {
        return std::make_shared<StreamerChannel>(std::move(fs));
    }
};

int main() {
    auto endpoint = IPEndpoint({{127, 0, 0, 1}}, 3000);
    auto factory = std::make_shared<StreamerChannelFactory>();
    auto processingThreads = 1;

    HttpServer server(endpoint, factory, processingThreads);
    Log::SetLevel(Log::Level::Info);

    auto task = server.Start();
    Log::Info("Streamer Server Started");
    task.wait();
}
