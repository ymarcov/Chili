#include "OrchestratedTcpServer.h"

namespace Yam {
namespace Http {

OrchestratedTcpServer::OrchestratedTcpServer(const IPEndpoint& ep, std::unique_ptr<ChannelFactory> factory) :
    TcpServer(ep),
    _orchestrator(std::make_shared<Orchestrator>(std::move(factory))) {
    _orchestrator->OnStop += [this] { Stop(); };
    _orchestrator->Start();
}

void OrchestratedTcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    conn->SetBlocking(false);
    _orchestrator->Add(std::move(conn));
}

} // namespace Http
} // namespace Yam

