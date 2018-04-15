#include "SocketServer.h"
#include "Log.h"
#include "SystemError.h"

#include <algorithm>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Chili {

SocketServer::~SocketServer() {
    Stop();
}

std::future<void> SocketServer::Start() {
    if (!_stop || _acceptThread.joinable() || _dispatchThread.joinable())
        throw std::logic_error("Start() called when socket server is already running");

    // reset state
    ResetListenerSocket(_socket);
    _stop = false;
    _promise = std::promise<void>{};

    _acceptThread = std::thread([this] { AcceptLoop(); });
    _dispatchThread = std::thread([this] { DispatchLoop(); });

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
        } else {
            if (_stop) {
                // OK: we were supposed to stop
                _promise.set_value();
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

    // all work is done. notify future.
    _promise.set_value();
}

void SocketServer::DispatchLoop() {

    while (!_stop) {
        _semaphore.Decrement();

        if (_stop)
            return;

        std::unique_lock lock(_mutex);

        auto fd = _acceptedFds.front();
        _acceptedFds.pop();

        lock.unlock();

        try {
            OnAccepted(fd);
        } catch (...) {
            Log::Warning("Socket server OnAccepted() threw an error which was ignored. Please handle internally!");
        }
    }
}

void SocketServer::Stop() {
    _stop = true;
    _socket = SocketStream{};

    _semaphore.Increment();

    if (_acceptThread.joinable())
        _acceptThread.join();

    if (_dispatchThread.joinable())
        _dispatchThread.join();
}

} // namespace Chili

