#pragma once

#include "Orchestrator.h"
#include "TcpAcceptor.h"

namespace Chili {

/**
 * An HTTP/TCP server.
 */
class HttpServer {
public:
    /**
     * Creates a new server listening on \p endpoint.
     *
     * @param endpoint        The IP endpoint to listen on.
     *
     * @param channelFactory  The factory from which to create new
     *                        channels when connections are received.
     */
    HttpServer(const IPEndpoint& endpoint, std::shared_ptr<ChannelFactory> channelFactory, int listeners = 1);

    /**
     * Starts the server so that new connections can be accepted.
     *
     * @return A task that finishes when the server has stopped.
     */
    std::future<void> Start();

    /**
     * Request the server to stop. This should cause the task
     * returned by Start() to finish.
     */
    void Stop();

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
    TcpAcceptor _tcpAcceptor;
    std::shared_ptr<Orchestrator> _orchestrator;
};

} // namespace Chili

