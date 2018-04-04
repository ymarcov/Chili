#include "HttpServer.h"

namespace Chili {

HttpServer::HttpServer(const IPEndpoint& ep, std::shared_ptr<ChannelFactory> factory) :
    TcpServer(ep),
    _orchestrator(Orchestrator::Create(std::move(factory), 8)) {
    _orchestrator->OnStop += [this] { Stop(); };
    _orchestrator->Start();
}

void HttpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    conn->SetBlocking(false);
    _orchestrator->Add(std::move(conn));
}

void HttpServer::ThrottleRead(Throttler t) {
    _orchestrator->ThrottleRead(std::move(t));
}

void HttpServer::ThrottleWrite(Throttler t) {
    _orchestrator->ThrottleWrite(std::move(t));
}

void HttpServer::SetInactivityTimeout(std::chrono::milliseconds ms) {
    _orchestrator->SetInactivityTimeout(std::move(ms));
}

} // namespace Chili

