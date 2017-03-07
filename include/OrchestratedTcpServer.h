#pragma once

#include "Orchestrator.h"
#include "TcpServer.h"

namespace Yam {
namespace Http {

class OrchestratedTcpServer : public TcpServer {
public:
    OrchestratedTcpServer(const IPEndpoint&, std::shared_ptr<Orchestrator>);

protected:
    void OnAccepted(std::shared_ptr<TcpConnection>);

private:
    std::shared_ptr<Orchestrator> _orchestrator;
};

} // namespace Http
} // namespace Yam

