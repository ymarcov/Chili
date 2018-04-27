#include "TcpConnection.h"
#include "SystemError.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Chili {

namespace {

void Connect(SocketStream& socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    auto paddr = reinterpret_cast<::sockaddr*>(addr.get());
    if (-1 == ::connect(socket.GetNativeHandle(), paddr, sizeof(*addr)))
        throw SystemError{};
}

} // unnamed namespace

TcpConnection::TcpConnection(const IPEndpoint& ep) :
    SocketStream{::socket(AF_INET, SOCK_STREAM, 0)},
    _endpoint{ep} {
    Connect(*this, _endpoint);
}

TcpConnection::TcpConnection(SocketStream s, const IPEndpoint& ep) :
    SocketStream{std::move(s)},
    _endpoint{ep} {}

void TcpConnection::Cork(bool enabled) {
    int value = enabled ? 1 : 0;
    if (-1 == ::setsockopt(_nativeHandle, IPPROTO_TCP, TCP_CORK, &value, sizeof(value)))
        throw SystemError();
}

void TcpConnection::Flush() {
    int originalValue;
    ::socklen_t len;

    if (-1 == ::getsockopt(_nativeHandle, IPPROTO_TCP, TCP_NODELAY, &originalValue, &len))
        throw SystemError();

    int onValue = 1;

    if (-1 == ::setsockopt(_nativeHandle, IPPROTO_TCP, TCP_NODELAY, &onValue, sizeof(len))) {
        auto e = SystemError();
        ::setsockopt(_nativeHandle, IPPROTO_TCP, TCP_NODELAY, &originalValue, sizeof(len));
        throw e;
    }

    if (-1 == ::setsockopt(_nativeHandle, IPPROTO_TCP, TCP_NODELAY, &originalValue, sizeof(len)))
        throw SystemError();
}

} // namespace Chili

