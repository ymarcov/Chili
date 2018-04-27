#pragma once

#include "IPEndpoint.h"
#include "SocketStream.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace Chili {

class TcpConnection : public SocketStream {
public:
    TcpConnection(const IPEndpoint&);
    TcpConnection(SocketStream, const IPEndpoint&);

    const IPEndpoint& Endpoint() const noexcept;
    void Cork(bool);
    void Flush();

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

} // namespace Chili

