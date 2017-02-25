#include "SocketServer.h"
#include "SystemError.h"

#include <algorithm>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Yam {
namespace Http {

SocketServer::~SocketServer() {
    Stop();
}

std::future<void> SocketServer::Start() {
    if (!_stop || _thread.joinable())
        throw std::logic_error("Start() called when socket server is already running");

    // reset state
    ResetListenerSocket(_socket);
    _stop = false;
    _promise = std::promise<void>{};

    _thread = std::thread([this] { AcceptLoop(); });

    return _promise.get_future();
}

void SocketServer::AcceptLoop() {
    while (!_stop) {
        // block until a new connection is accepted
        int ret = ::accept(_socket.GetNativeHandle(),
                            reinterpret_cast<::sockaddr*>(AddressBuffer()),
                            reinterpret_cast<::socklen_t*>(AddressBufferSize()));

        if (ret == -1) {
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
                    return; // TODO: log this
                } else {
                    continue;
                }
            }
        }

        try {
            OnAccepted(ret);
        } catch (...) {
            // TODO: log this
        }
    }

    // all work is done. notify future.
    _promise.set_value();
}

void SocketServer::Stop() {
    _stop = true;
    _socket = SocketStream{};

    if (_thread.joinable())
        _thread.join();
}

} // namespace Http
} // namespace Yam

