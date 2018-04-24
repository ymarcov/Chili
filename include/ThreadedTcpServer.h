#pragma once

#include "TcpAcceptor.h"
#include "ThreadPool.h"

#include <functional>
#include <memory>

namespace Chili {

class ThreadedTcpServer {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<TcpConnection>)>;

    ThreadedTcpServer(const IPEndpoint&, std::shared_ptr<ThreadPool>);

    std::future<void> Start(ConnectionHandler);

    void Stop();

private:
    TcpAcceptor _tcpAcceptor;
    std::shared_ptr<ThreadPool> _threadPool;
    ConnectionHandler _handler;
};

} // namespace Chili

