#pragma once

#include "SocketServer.h"
#include "SocketStream.h"
#include "TcpConnection.h"

#include <atomic>
#include <future>
#include <memory>
#include <netinet/in.h>
#include <thread>

namespace Nitra {

/**
 * A TCP server.
 */
class TcpServer : public SocketServer {
public:
    TcpServer(const IPEndpoint&);

    /**
     * Gets the endpoint that this server
     * is listening on.
     */
    const IPEndpoint& GetEndpoint() const;

protected:
    virtual void OnAccepted(std::shared_ptr<TcpConnection>) = 0;

private:
    void OnAccepted(int fd) override;
    void ResetListenerSocket(SocketStream&) override;
    void* AddressBuffer() override;
    std::size_t* AddressBufferSize() override;

    IPEndpoint _endpoint;
    ::sockaddr_in _addrBuffer;
    std::size_t _addrBufferSize = sizeof(::sockaddr_in);
};

inline const IPEndpoint& TcpServer::GetEndpoint() const {
    return _endpoint;
}

} // namespace Nitra

