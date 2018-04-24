#pragma once

#include "TcpAcceptor.h"
#include "Poller.h"

#include <memory>

namespace Chili {

class PolledTcpServer {
public:
    PolledTcpServer(const IPEndpoint&, std::shared_ptr<Poller>);

    /**
     * Starts the server so that new connections can be accepted.
     *
     * @return A task that finishes when the server has stopped.
     */
    std::future<void> Start();

    /**
     * Request the server to stop. This should cause the task
     * returned by Start() to finish.
     */
    void Stop();

    const IPEndpoint& GetEndpoint() const;

private:
    TcpAcceptor _tcpAcceptor;
    std::shared_ptr<Poller> _poller;
};

} // namespace Chili

