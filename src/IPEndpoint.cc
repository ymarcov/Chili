#include "IPEndpoint.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <cstring>

namespace Chili {

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


} // namespace Chili

