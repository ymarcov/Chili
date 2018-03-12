#include "PolledTcpServer.h"

namespace Nitra {

PolledTcpServer::PolledTcpServer(const IPEndpoint& ep, std::shared_ptr<Poller> poller) :
    TcpServer{ep},
    _poller{std::move(poller)} {
    _poller->OnStop += [this] { Stop(); };
}

void PolledTcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    conn->SetBlocking(false);
    auto events = Poller::Events::Completion | Poller::Events::Readable;
    _poller->Poll(std::move(conn), events);
}

} // namespace Nitra

