#pragma once

#include "SocketStream.h"
#include "TcpConnection.h"
#include "ThreadPool.h"

#include <atomic>
#include <future>
#include <memory>
#include <thread>

namespace Yam {
namespace Http {

class TcpServer {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<TcpConnection>)>;

    TcpServer(const IPEndpoint&, std::shared_ptr<ThreadPool>);
    ~TcpServer();

    std::future<void> Start(ConnectionHandler);
    void Stop();

private:
    void ResetListenerSocketStream();
    void AcceptLoop(ConnectionHandler);

    IPEndpoint _endpoint;
    std::shared_ptr<ThreadPool> _threadPool;
    SocketStream _socket;
    std::promise<void> _promise;
    std::thread _thread;
    std::atomic_bool _stop;
};

} // namespace Http
} // namespace Yam

