#pragma once

#include "SocketServer.h"
#include "SocketStream.h"
#include "TcpConnection.h"

#include <atomic>
#include <future>
#include <memory>
#include <thread>

namespace Yam {
namespace Http {

class TcpServer : public SocketServer {
public:
    TcpServer(const IPEndpoint&);

    const IPEndpoint& GetEndpoint() const;

protected:
    virtual void OnAccepted(std::shared_ptr<TcpConnection>) = 0;

private:
    void OnAccepted(int fd) override;
    void ResetListenerSocket(SocketStream&) override;
    void* AddressBuffer() override;
    std::size_t* AddressBufferSize() override;

    IPEndpoint _endpoint;
};

inline const IPEndpoint& TcpServer::GetEndpoint() const {
    return _endpoint;
}

} // namespace Http
} // namespace Yam

