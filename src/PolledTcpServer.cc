#include "PolledTcpServer.h"

namespace Chili {

PolledTcpServer::PolledTcpServer(const IPEndpoint& ep, std::shared_ptr<Poller> poller) :
    _tcpAcceptor{ep, 1},
    _poller{std::move(poller)} {
    _tcpAcceptor.OnAccepted += [this](auto conn) {
        conn->SetBlocking(false);
        auto events = Poller::Events::Completion | Poller::Events::Readable;
        _poller->Poll(std::move(conn), events);
    };

    _poller->OnStop += [this] { Stop(); };
}

std::future<void> PolledTcpServer::Start() {
    return _tcpAcceptor.Start();
}

void PolledTcpServer::Stop() {
    return _tcpAcceptor.Stop();
}

const IPEndpoint& PolledTcpServer::GetEndpoint() const {
    return _tcpAcceptor.GetEndpoint();
}

} // namespace Chili

