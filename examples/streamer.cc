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

int main() {
    auto endpoint = IPEndpoint({{127, 0, 0, 1}}, 3000);

    auto factory = ChannelFactory::Create([](Channel& c) {
        c.ThrottleWrite({1024 * 1024, 1s});

        auto& res = c.GetResponse();

        try {
            std::string uri(c.GetRequest().GetUri());
            std::shared_ptr<FileStream> stream = FileStream::Open(uri, FileMode::Read);
            res.SetContent(stream);
            res.AppendField("Content-Type", "application/octet-stream");
            Log::Info("HELLO");
            res.SetStatus(Status::Ok);
            res.CloseConnection();
            c.SendResponse();
        } catch (const std::exception& ex) {
            Log::Error("Error: {}", ex.what());
            res.Reset();
            res.SetStatus(Status::InternalServerError);
            res.CloseConnection();
            c.SendResponse();
        }
    });

    HttpServer server(endpoint, factory);
    Log::SetLevel(Log::Level::Verbose);

    auto task = server.Start();
    Log::Info("Streamer Server Started");
    task.wait();
}
