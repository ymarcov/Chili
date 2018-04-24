#pragma once

#include "Profiler.h"
#include "Semaphore.h"
#include "SocketStream.h"

#include <atomic>
#include <exception>
#include <future>
#include <memory>
#include <queue>
#include <thread>

namespace Chili {

/**
 * A socket server.
 */
class Acceptor {
public:
    Acceptor(int listeners);
    virtual ~Acceptor();

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
    virtual void RelinquishSocket(int fd) = 0;
    virtual void ResetListenerSocket(SocketStream&) = 0;
    virtual void* AddressBuffer() = 0;
    virtual std::size_t* AddressBufferSize() = 0;

private:
    void AcceptLoop(int listener);
    void DispatchLoop();

    int _listeners;
    std::vector<SocketStream> _listenerSockets;
    std::promise<void> _promise;
    std::exception_ptr _promiseException;
    std::queue<int> _acceptedFds;
    std::unique_ptr<Semaphore> _semaphore;
    std::mutex _mutex;
    std::mutex _startStopMutex;
    std::vector<std::thread> _listenerThreads;
    std::thread _dispatchThread;
    std::atomic_bool _stop{true};
};

/**
 * Profiling
 */

class AcceptorEvent : public ProfileEvent {
public:
    std::string GetSource() const override;
    std::string GetSummary() const override;
    void Accept(ProfileEventReader&) const override;
};

class SocketQueued : public AcceptorEvent {
public:
    using AcceptorEvent::AcceptorEvent;
    void Accept(ProfileEventReader&) const override;
};

class SocketDequeued : public AcceptorEvent {
public:
    using AcceptorEvent::AcceptorEvent;
    void Accept(ProfileEventReader&) const override;
};

class SocketAccepted : public AcceptorEvent {
public:
    using AcceptorEvent::AcceptorEvent;
    void Accept(ProfileEventReader&) const override;
};

} // namespace Chili

