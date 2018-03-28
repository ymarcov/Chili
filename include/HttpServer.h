#pragma once

#include "Orchestrator.h"
#include "TcpServer.h"

namespace Chili {

/**
 * An HTTP/TCP server.
 */
class HttpServer : public TcpServer {
public:
    /**
     * Creates a new server listening on \p endpoint.
     *
     * @param endpoint        The IP endpoint to listen on.
     *
     * @param channelFactory  The factory from which to create new
     *                        channels when connections are received.
     *
     * @param threads         The number of threads to be used for
     *                        processing incoming requests.
     */
    HttpServer(const IPEndpoint& endpoint,
               std::shared_ptr<ChannelFactory> channelFactory,
               int threads);

    /**
     * Limit the read (input) bandwidth rate of this server.
     * This sets a hard limit over all channels as a whole.
     */
    void ThrottleRead(Throttler);

    /**
     * Limit the write (output) bandwidth rate of this server.
     * This sets a hard limit over all channels as a whole.
     */
    void ThrottleWrite(Throttler);

    /**
     * Sets the maximum duration a channel can remain
     * connected without being active at all (i.e.
     * not sending, reading, or processing any data).
     */
    void SetInactivityTimeout(std::chrono::milliseconds);

private:
    void OnAccepted(std::shared_ptr<TcpConnection>) override;

    std::shared_ptr<Orchestrator> _orchestrator;
};

} // namespace Chili

