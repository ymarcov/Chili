#pragma once

#include "TcpServerBase.h"
#include "ThreadPool.h"

#include <functional>
#include <memory>

namespace Yam {
namespace Http {

class ThreadedTcpServer : public TcpServerBase {
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

} // namespace Http
} // namespace Yam

