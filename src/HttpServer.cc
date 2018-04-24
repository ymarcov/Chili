#include "HttpServer.h"

namespace Chili {

HttpServer::HttpServer(const IPEndpoint& ep, std::shared_ptr<ChannelFactory> factory, int listeners) :
    _tcpAcceptor(ep, listeners),
    _orchestrator(Orchestrator::Create(std::move(factory), 8)) {
    _tcpAcceptor.OnAccepted += [this](auto conn) {
        conn->SetBlocking(false);
        _orchestrator->Add(std::move(conn));
    };

    _orchestrator->OnStop += [this] { Stop(); };
    _orchestrator->Start();
}

std::future<void> HttpServer::Start() {
    return _tcpAcceptor.Start();
}

void HttpServer::Stop() {
    _tcpAcceptor.Stop();
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

