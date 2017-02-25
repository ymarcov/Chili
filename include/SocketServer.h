#pragma once

#include "SocketStream.h"

#include <atomic>
#include <future>
#include <memory>
#include <thread>

namespace Yam {
namespace Http {

class SocketServer {
public:
    virtual ~SocketServer();

    std::future<void> Start();
    void Stop();

protected:
    virtual void OnAccepted(int fd) = 0;
    virtual void ResetListenerSocket(SocketStream&) = 0;
    virtual void* AddressBuffer() = 0;
    virtual std::size_t* AddressBufferSize() = 0;

private:
    void AcceptLoop();

    SocketStream _socket;
    std::promise<void> _promise;
    std::thread _thread;
    std::atomic_bool _stop{true};
};

} // namespace Http
} // namespace Yam

