#include "TcpServer.h"
#include "SystemError.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace Yam {
namespace Http {

namespace {

class SocketGuard {
public:
    SocketGuard(int s) :
        _s(s) {}

    ~SocketGuard() {
        if (_s != -1)
            ::close(_s);
    }

    int Release() {
        int s = _s;
        _s = -1;
        return s;
    }

    operator int() const { return _s; }

private:
    int _s;
};

bool EnableAddressReuse(int socket) {
    int opt = 1;
    return -1 != ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

bool BindTo(int socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    return -1 != ::bind(socket, reinterpret_cast<::sockaddr*>(addr.get()), sizeof(*addr));
}

} // unnamed namespace

TcpServer::TcpServer(const IPEndpoint& ep, std::shared_ptr<ThreadPool> tp) :
    _endpoint(ep),
    _threadPool(std::move(tp)),
    _socket(-1),
    _stop(true) {}

TcpServer::~TcpServer() {
    Stop();
}

int TcpServer::CreateListenerSocket() const {
    SocketGuard sock = ::socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
        throw SystemError();

    if (!EnableAddressReuse(sock))
        throw SystemError();

    if (!BindTo(sock, _endpoint))
        throw SystemError();

    return sock.Release();
}

std::future<void> TcpServer::Start(ConnectionHandler ch) {
    if (_thread.joinable())
        throw std::logic_error("Start() called when TCP server is already running");

    _socket = CreateListenerSocket();

    if (-1 == ::listen(_socket, SOMAXCONN))
        throw SystemError();

    // reset state
    _stop = false;
    _promise = std::promise<void>();

    // dispatch background worker thread
    _thread = std::thread([=] {
        AcceptLoop(std::move(ch));
    });

    return _promise.get_future();
}

void TcpServer::AcceptLoop(ConnectionHandler connectionHandler) {
    while (!_stop) {
        ::sockaddr_in saddr{0};
        ::socklen_t saddr_size = sizeof(saddr);

        // block until a new connection is accepted
        int ret = ::accept(_socket, reinterpret_cast<::sockaddr*>(&saddr), &saddr_size);

        if (ret == -1) {
            if (_stop) {
                // OK: we were supposed to stop
                _promise.set_value();
            } else {
                // Oops: something bad happened
                _stop = true;
                _promise.set_exception(std::make_exception_ptr(SystemError{}));
            }
            return;
        }

        // we've got a new connection at hand;
        // trigger connection handler callback.

        _threadPool->Post([=] {
            IPEndpoint endpoint{saddr};
            auto conn = std::make_shared<TcpConnection>(ret, endpoint);
            connectionHandler(std::move(conn));
        });
    }

    // all work is done. notify future.
    _promise.set_value();
}

void TcpServer::Stop() {
    _stop = true;

    if (_socket != -1) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }

    if (_thread.joinable())
        _thread.join();
}

} // namespace Http
} // namespace Yam

