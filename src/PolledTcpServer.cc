#include "PolledTcpServer.h"

namespace Yam {
namespace Http {

PolledTcpServer::PolledTcpServer(const IPEndpoint& ep, std::shared_ptr<Poller> poller) :
    TcpServer{ep},
    _poller{std::move(poller)} {
    _poller->OnStop += [this] { Stop(); };
}

void PolledTcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    conn->SetBlocking(false);
    _poller->Register(std::move(conn));
}

} // namespace Http
} // namespace Yam

