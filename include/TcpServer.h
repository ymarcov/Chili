#pragma once

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
    int CreateListenerSocket() const;
    void AcceptLoop(ConnectionHandler);

    IPEndpoint _ep;
    std::shared_ptr<ThreadPool> _tp;
    int _socket;
    std::promise<void> _promise;
    std::thread _thread;
    std::atomic_bool _stop;
};

} // namespace Http
} // namespace Yam

