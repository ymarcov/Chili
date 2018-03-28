#pragma once

#include "TcpServer.h"
#include "Poller.h"

#include <memory>

namespace Chili {

class PolledTcpServer : public TcpServer {
public:
    PolledTcpServer(const IPEndpoint&, std::shared_ptr<Poller>);

protected:
    void OnAccepted(std::shared_ptr<TcpConnection>);

private:
    std::shared_ptr<Poller> _poller;
};

} // namespace Chili

