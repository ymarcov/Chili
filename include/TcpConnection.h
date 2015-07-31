#pragma once

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

class TcpConnection {
public:
    TcpConnection(const IPEndpoint&);
    TcpConnection(int socket, const IPEndpoint&);
    ~TcpConnection();

    void Write(const void*, std::size_t);
    std::size_t Read(void*, std::size_t);
    const IPEndpoint& Endpoint() const noexcept;
    int NativeHandle() const noexcept;

private:
    int _socket;
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

inline int TcpConnection::NativeHandle() const noexcept {
    return _socket;
}

} // namespace Http
} // namespace Yam

