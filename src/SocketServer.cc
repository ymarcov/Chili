#include "SocketServer.h"
#include "ExitTrap.h"
#include "Log.h"
#include "SystemError.h"

#include <algorithm>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Chili {

SocketServer::SocketServer(int listeners)
    : _listenerThreads(listeners) {}

SocketServer::~SocketServer() {
    Stop();
}

std::future<void> SocketServer::Start() {
    if (!_stop || _dispatchThread.joinable())
        throw std::logic_error("Start() called when socket server is already running");

    // reset state
    ResetListenerSocket(_socket);
    _stop = false;
    _promise = std::promise<void>{};

    _dispatchThread = std::thread([this] { DispatchLoop(); });

    for (auto& t : _listenerThreads)
        t = std::thread([this] { AcceptLoop(); });

    return _promise.get_future();
}

void SocketServer::AcceptLoop() {
    while (!_stop) {
        // block until a new connection is accepted
        int ret = ::accept(_socket.GetNativeHandle(),
                           reinterpret_cast<::sockaddr*>(AddressBuffer()),
                           reinterpret_cast<::socklen_t*>(AddressBufferSize()));

        if (ret > 0) {
            std::lock_guard lock(_mutex);
            _acceptedFds.push(ret);
            _semaphore.Increment();
            Profiler::Record<SocketQueued>();
        } else {
            if (_stop) {
                // OK: we were supposed to stop
                return;
            } else {
                // Oops: something bad happened

                auto ignored = {
                    ECONNABORTED,
                    EMFILE,
                    ENFILE,
                    ENOBUFS,
                    ENOMEM,
                    EPROTO,
                    EPERM
                };

                if (std::find(begin(ignored), end(ignored), ret) == end(ignored)) {
                    _stop = true;
                    _promise.set_exception(std::make_exception_ptr(SystemError{}));
                    Log::Error("Socket server closed due to unrecoverable error");
                    return;
                } else {
                    continue;
                }
            }
        }
    }
}

void SocketServer::DispatchLoop() {
    auto onExit = CreateExitTrap([&] {
        // all work is done. notify future.
        _promise.set_value();
    });

    while (!_stop) {
        _semaphore.Decrement();

        if (_stop)
            return;

        std::unique_lock lock(_mutex);

        auto fd = _acceptedFds.front();
        _acceptedFds.pop();

        lock.unlock();

        Profiler::Record<SocketDequeued>();

        try {
            OnAccepted(fd);
            Profiler::Record<SocketAccepted>();
        } catch (...) {
            Log::Warning("Socket server OnAccepted() threw an error which was ignored. Please handle internally!");
        }
    }
}

void SocketServer::Stop() {
    _stop = true;
    _socket = SocketStream{};

    _semaphore.Increment();

    for (auto& t : _listenerThreads)
        if (t.joinable())
            t.join();

    if (_dispatchThread.joinable())
        _dispatchThread.join();
}

std::string SocketServerEvent::GetSource() const {
    return "SocketServer";
}

std::string SocketServerEvent::GetSummary() const {
    return "Event on SocketServer";
}

void SocketServerEvent::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void SocketQueued::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void SocketDequeued::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void SocketAccepted::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

} // namespace Chili

