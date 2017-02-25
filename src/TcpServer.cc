#include "TcpServer.h"
#include "SystemError.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace Yam {
namespace Http {

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

TcpServer::TcpServer(const IPEndpoint& ep) :
    _endpoint{ep} {}

void TcpServer::ResetListenerSocket(SocketStream& socket) {
    socket = SocketStream();

    auto fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (fd == -1)
        throw SystemError();

    socket = SocketStream{fd};

    EnableAddressReuse(socket);
    BindTo(socket, _endpoint);
    Listen(socket);
}

void TcpServer::OnAccepted(int fd) {
    OnAccepted(std::make_shared<TcpConnection>(fd, *static_cast<::sockaddr_in*>(AddressBuffer())));
}

void* TcpServer::AddressBuffer() {
    static ::sockaddr_in addr;
    return &addr;
}

std::size_t* TcpServer::AddressBufferSize() {
    static auto size = sizeof(::sockaddr_in);
    return &size;
}

} // namespace Http
} // namespace Yam

