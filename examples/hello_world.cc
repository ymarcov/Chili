#include "Chili.h"

int main() {
    auto endpoint = Chili::IPEndpoint({{127, 0, 0, 1}}, 3000);

    auto handler = Chili::RequestHandler([](Chili::Channel& c) {
        auto& res = c.GetResponse();

        res.SetContent("<h1>Hello world!</h1>");
        res.AppendHeader("Content-Type", "text/html");
        res.SetStatus(Chili::Status::Ok);

        c.SendResponse();
    });

    auto server = Chili::HttpServer(endpoint, handler);

    auto task = server.Start();
    Chili::Log::Info("HelloWorld Server Started");
    task.wait();
}
