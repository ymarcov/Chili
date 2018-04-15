#pragma once

#include "Semaphore.h"
#include "SocketStream.h"

#include <atomic>
#include <future>
#include <memory>
#include <queue>
#include <thread>

namespace Chili {

/**
 * A socket server.
 */
class SocketServer {
public:
    virtual ~SocketServer();

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

protected:
    virtual void OnAccepted(int fd) = 0;
    virtual void ResetListenerSocket(SocketStream&) = 0;
    virtual void* AddressBuffer() = 0;
    virtual std::size_t* AddressBufferSize() = 0;

private:
    void AcceptLoop();
    void DispatchLoop();

    SocketStream _socket;
    std::promise<void> _promise;
    std::queue<int> _acceptedFds;
    Semaphore _semaphore;
    std::mutex _mutex;
    std::thread _acceptThread;
    std::thread _dispatchThread;
    std::atomic_bool _stop{true};
};

} // namespace Chili

