#include "TcpConnection.h"
#include "SystemError.h"

#include <cstdint>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Yam {
namespace Http {

namespace {

bool ConnectTo(int socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    return -1 != ::connect(socket, reinterpret_cast<::sockaddr*>(addr.get()), sizeof(*addr));
}

} // unnamed namespace

IPEndpoint::IPEndpoint(const ::sockaddr_in& sa) {
    int port = ::ntohs(sa.sin_port);
    auto ip = reinterpret_cast<const std::uint8_t*>(&sa.sin_addr.s_addr);
    _address = {ip[0], ip[1], ip[2], ip[3]};
    _port = port;
}

std::unique_ptr<::sockaddr_in> IPEndpoint::GetAddrInfo() const {
    auto addr = std::make_unique<::sockaddr_in>();
    std::memset(addr.get(), 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = ::htons(_port);
    std::uint32_t ip = *reinterpret_cast<const std::uint32_t*>(_address.data());
    addr->sin_addr.s_addr = ip;
    return std::move(addr);
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
    _socket(::socket(AF_INET, SOCK_STREAM, 0)),
    _ep{ep} {
    if (_socket == -1) throw SystemError();
    if (!ConnectTo(_socket, ep)) throw SystemError();
}

TcpConnection::TcpConnection(int socket, const IPEndpoint& ep) :
    _socket(socket),
    _ep{ep} {}

TcpConnection::~TcpConnection() {
    if (_socket != -1) ::close(_socket);
}

void TcpConnection::Write(const void* data, std::size_t size) {
    if (-1 == ::send(_socket, data, size, MSG_NOSIGNAL))
        throw SystemError();
}

std::size_t TcpConnection::Read(void* buffer, std::size_t size) {
    int n = ::recv(_socket, buffer, size, 0);
    if (n == -1) throw SystemError();
    return n;
}

} // namespace Http
} // namespace Yam

