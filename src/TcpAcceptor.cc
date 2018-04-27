#include "TcpAcceptor.h"
#include "SystemError.h"

#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace Chili {

namespace {

void EnableAddressReuse(SocketStream& socket) {
    int optValue = 1;
    if( -1 == ::setsockopt(socket.GetNativeHandle(),
                             SOL_SOCKET,
                             SO_REUSEADDR,
                             &optValue,
                             sizeof(optValue))) {
        throw SystemError{};
    }
}

void EnablePortReuse(SocketStream& socket) {
    int optValue = 1;
    if( -1 == ::setsockopt(socket.GetNativeHandle(),
                             SOL_SOCKET,
                             SO_REUSEPORT,
                             &optValue,
                             sizeof(optValue))) {
        throw SystemError{};
    }
}

void BindTo(SocketStream& socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    auto paddr = reinterpret_cast<::sockaddr*>(addr.get());
    if (-1 == ::bind(socket.GetNativeHandle(), paddr, sizeof(*addr)))
        throw SystemError{};
}

void Listen(SocketStream& socket) {
    if (-1 == ::listen(socket.GetNativeHandle(), SOMAXCONN))
        throw SystemError{};
}

} // unnamed namespace

TcpAcceptor::TcpAcceptor(const IPEndpoint& ep, int listeners)
    : Acceptor{listeners}
    , _endpoint{ep} {}

void TcpAcceptor::ResetListenerSocket(SocketStream& socket) {
    socket = SocketStream();

    auto fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (fd == -1)
        throw SystemError();

    socket = SocketStream{fd};

    EnableAddressReuse(socket);
    EnablePortReuse(socket);
    BindTo(socket, _endpoint);
    Listen(socket);
}

void TcpAcceptor::RelinquishSocket(int fd, IPEndpoint ep) {
    OnAccepted(std::make_shared<TcpConnection>(fd, std::move(ep)));
}

void* TcpAcceptor::AddressBuffer() {
    return &_addrBuffer;
}

std::size_t* TcpAcceptor::AddressBufferSize() {
    return &_addrBufferSize;
}

} // namespace Chili

