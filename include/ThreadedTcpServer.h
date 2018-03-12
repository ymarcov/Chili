#pragma once

#include "TcpServer.h"
#include "ThreadPool.h"

#include <functional>
#include <memory>

namespace Nitra {

class ThreadedTcpServer : public TcpServer {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<TcpConnection>)>;

    ThreadedTcpServer(const IPEndpoint&, std::shared_ptr<ThreadPool>);

    std::future<void> Start(ConnectionHandler);

protected:
    void OnAccepted(std::shared_ptr<TcpConnection>) override;

private:
    std::shared_ptr<ThreadPool> _threadPool;
    ConnectionHandler _handler;
};

} // namespace Nitra

