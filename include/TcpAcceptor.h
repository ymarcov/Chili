#pragma once

#include "Signal.h"
#include "Acceptor.h"
#include "SocketStream.h"
#include "TcpConnection.h"

#include <atomic>
#include <future>
#include <memory>
#include <netinet/in.h>
#include <thread>

namespace Chili {

/**
 * A TCP server.
 */
class TcpAcceptor : public Acceptor {
public:
    TcpAcceptor(const IPEndpoint&, int listeners);

    /**
     * Gets the endpoint that this server
     * is listening on.
     */
    const IPEndpoint& GetEndpoint() const;

    Signal<std::shared_ptr<TcpConnection>> OnAccepted;

private:
    void RelinquishSocket(int fd) override;
    void ResetListenerSocket(SocketStream&) override;
    void* AddressBuffer() override;
    std::size_t* AddressBufferSize() override;

    IPEndpoint _endpoint;
    ::sockaddr_in _addrBuffer;
    std::size_t _addrBufferSize = sizeof(::sockaddr_in);
};

inline const IPEndpoint& TcpAcceptor::GetEndpoint() const {
    return _endpoint;
}

} // namespace Chili

