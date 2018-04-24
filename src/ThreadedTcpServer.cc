#include "ThreadedTcpServer.h"

namespace Chili {

ThreadedTcpServer::ThreadedTcpServer(const IPEndpoint& ep, std::shared_ptr<ThreadPool> tp) :
    _tcpAcceptor{ep, 1},
    _threadPool{std::move(tp)} {
    _tcpAcceptor.OnAccepted += [this](auto conn) {
        _threadPool->Post([conn=std::move(conn), handler=_handler] {
            handler(std::move(conn));
        });
    };
}

std::future<void> ThreadedTcpServer::Start(ThreadedTcpServer::ConnectionHandler handler) {
    _handler = std::move(handler);
    return _tcpAcceptor.Start();
}

void ThreadedTcpServer::Stop() {
    return _tcpAcceptor.Stop();
}

} // namespace Chili

