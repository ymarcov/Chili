#include "OrchestratedTcpServer.h"

namespace Yam {
namespace Http {

OrchestratedTcpServer::OrchestratedTcpServer(const IPEndpoint& ep, std::shared_ptr<Orchestrator> orchestrator) :
    TcpServer{ep},
    _orchestrator{std::move(orchestrator)} {
    _orchestrator->OnStop += [this] { Stop(); };
}

void OrchestratedTcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    conn->SetBlocking(false);
    _orchestrator->Add(std::move(conn));
}

} // namespace Http
} // namespace Yam

