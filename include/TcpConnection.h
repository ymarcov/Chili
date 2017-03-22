#pragma once

#include "SocketStream.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

struct sockaddr_in;

namespace Yam {
namespace Http {

class IPEndpoint {
public:
    IPEndpoint(const std::array<std::uint8_t, 4> address, int port);
    IPEndpoint(const ::sockaddr_in&);

    const std::array<std::uint8_t, 4>& GetAddress() const noexcept;
    int GetPort() const noexcept;
    std::unique_ptr<::sockaddr_in> GetAddrInfo() const;
    std::string ToString() const;

private:
    std::array<std::uint8_t, 4> _address;
    int _port;
};

class TcpConnection : public SocketStream {
public:
    TcpConnection(const IPEndpoint&);
    TcpConnection(SocketStream, const IPEndpoint&);

    const IPEndpoint& Endpoint() const noexcept;
    void Cork(bool);

private:
    IPEndpoint _endpoint;
};

inline IPEndpoint::IPEndpoint(const std::array<std::uint8_t, 4> address, int port) :
    _address(address),
    _port{port} {}

inline const std::array<std::uint8_t, 4>& IPEndpoint::GetAddress() const noexcept {
    return _address;
}

inline int IPEndpoint::GetPort() const noexcept {
    return _port;
}

inline const IPEndpoint& TcpConnection::Endpoint() const noexcept {
    return _endpoint;
}

} // namespace Http
} // namespace Yam

