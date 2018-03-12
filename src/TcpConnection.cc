#include "TcpConnection.h"
#include "SystemError.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Nitra {

namespace {

void Connect(SocketStream& socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    auto paddr = reinterpret_cast<::sockaddr*>(addr.get());
    if (-1 == ::connect(socket.GetNativeHandle(), paddr, sizeof(*addr)))
        throw SystemError{};
}

} // unnamed namespace

IPEndpoint::IPEndpoint(const ::sockaddr_in& sa) {
    int port = ntohs(sa.sin_port);
    auto ip = reinterpret_cast<const std::uint8_t*>(&sa.sin_addr.s_addr);
    _address = {{ip[0], ip[1], ip[2], ip[3]}};
    _port = port;
}

std::unique_ptr<::sockaddr_in> IPEndpoint::GetAddrInfo() const {
    auto addr = std::make_unique<::sockaddr_in>();
    std::memset(addr.get(), 0, sizeof(*addr));

    addr->sin_family = AF_INET;
    addr->sin_port = htons(_port);
    auto ip = *reinterpret_cast<const std::uint32_t*>(_address.data());
    addr->sin_addr.s_addr = ip;

    return addr;
}

std::string IPEndpoint::ToString() const {
    std::ostringstream os;

    os << (int)_address[0] << '.'
       << (int)_address[1] << '.'
       << (int)_address[2] << '.'
       << (int)_address[3] << ':'
       << _port;

    return os.str();
}

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

} // namespace Nitra

