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

bool ReuseAddress(int socket) {
    int opt = 1;
    return -1 != ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

bool BindTo(int socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    return -1 != ::bind(socket, reinterpret_cast<::sockaddr*>(addr.get()), sizeof(*addr));
}

} // unnamed namespace

TcpServer::TcpServer(const IPEndpoint& ep, std::shared_ptr<ThreadPool> tp) :
    _ep(ep),
    _tp(std::move(tp)),
    _socket(-1),
    _stop(true) {
}

TcpServer::~TcpServer() {
    Stop();
    if (_thread.joinable())
        _thread.join();
}

int TcpServer::CreateListenerSocket() const {
    struct SocketGuard {
        SocketGuard(int s) : _s(s) {}
        operator int() const { return _s; }
        int Release() { int s = _s; _s = -1; return s; }
        ~SocketGuard() { if (_s != -1) ::close(_s); }
        int _s;
    };

    SocketGuard s{::socket(AF_INET, SOCK_STREAM, 0)};
    if (s == -1) throw SystemError();
    if (!ReuseAddress(s)) throw SystemError();
    if (!BindTo(s, _ep)) throw SystemError();
    return s.Release();
}

std::future<void> TcpServer::Start(ConnectionHandler ch) {
    _socket = CreateListenerSocket();
    if (-1 == ::listen(_socket, SOMAXCONN)) throw SystemError();
    if (_thread.joinable()) _thread.join();
    _stop = false;
    _promise = std::promise<void>();
    _thread = std::thread([=] { AcceptLoop(std::move(ch)); });
    return _promise.get_future();
}

void TcpServer::AcceptLoop(ConnectionHandler ch) {
    while (!_stop) {
        ::sockaddr_in sa{0};
        ::socklen_t sa_size = sizeof(sa);
        int ret = ::accept(_socket, reinterpret_cast<::sockaddr*>(&sa), &sa_size);
        if (ret == -1) {
            if (_stop) {
                _promise.set_value();
            } else {
                _stop = true;
                _promise.set_exception(std::make_exception_ptr(SystemError{}));
            }
            return;
        }
        _tp->Post([&ch, ret, sa] {
            ch(std::make_shared<TcpConnection>(ret, IPEndpoint{sa}));
        });
    }
    _promise.set_value();
}

void TcpServer::Stop() {
    _stop = true;
    if (_socket != -1) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
    }
}

} // namespace Http
} // namespace Yam

