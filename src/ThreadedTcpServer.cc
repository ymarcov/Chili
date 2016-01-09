#include "ThreadedTcpServer.h"

namespace Yam {
namespace Http {

ThreadedTcpServer::ThreadedTcpServer(const IPEndpoint& ep, std::shared_ptr<ThreadPool> tp) :
    TcpServerBase{ep},
    _threadPool{std::move(tp)} {}

std::future<void> ThreadedTcpServer::Start(ThreadedTcpServer::ConnectionHandler handler) {
    _handler = std::move(handler);
    return TcpServerBase::Start();
}

void ThreadedTcpServer::OnAccepted(std::shared_ptr<TcpConnection> conn) {
    _threadPool->Post([conn=std::move(conn), handler=_handler] {
        handler(std::move(conn));
    });
}

} // namespace Http
} // namespace Yam

