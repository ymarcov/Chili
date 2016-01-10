#include "TcpServer.h"

namespace Yam {
namespace Http {

TcpServer::TcpServer(const IPEndpoint& ep, std::shared_ptr<Poller> poller) :
    TcpServerBase{ep},
    _poller{std::move(poller)} {
    _poller->OnStop += [this] { Stop(); };
}

void TcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    _poller->Register(std::move(conn));
}

} // namespace Http
} // namespace Yam

