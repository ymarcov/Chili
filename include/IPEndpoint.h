#pragma once

#include <array>
#include <memory>
#include <string>

struct sockaddr_in;

namespace Chili {

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


} // namespace Chili

