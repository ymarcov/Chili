#include "ThreadedTcpServer.h"

namespace Chili {

ThreadedTcpServer::ThreadedTcpServer(const IPEndpoint& ep, std::shared_ptr<ThreadPool> tp) :
    TcpServer{ep},
    _threadPool{std::move(tp)} {}

std::future<void> ThreadedTcpServer::Start(ThreadedTcpServer::ConnectionHandler handler) {
    _handler = std::move(handler);
    return TcpServer::Start();
}

void ThreadedTcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    _threadPool->Post([conn=std::move(conn), handler=_handler] {
        handler(std::move(conn));
    });
}

} // namespace Chili

