#pragma once

#include "Channel.h"

namespace Yam {
namespace Http {

/**
 * Base class for custom channels.
 *
 * A channel is an HTTP state machine on top
 * of an open socket.
 */
class ChannelBase : public Channel {
public:
    using Channel::Channel;

    /**
     * Sets whether to automatically get each request's entire
     * message content, regardless of how long it is, before
     * calling Process().
     *
     * This is true by default.
     *
     * This may only be called in the channel's constructor.
     * Otherwise, the behavior is undefined.
     */
    void SetAutoFetchContent(bool);

    /**
     * Gets the request associated with the current processing call.
     */
    Request& GetRequest();

    /**
     * Gets the responder associated with the current processing call.
     */
    Responder& GetResponder();

    /**
     * Instructs the server to fetch the rest of the content
     * (message body) of the request being processed.
     */
    Control FetchContent();

    /**
     * Instructs the server to reject the rest of the content
     * (message body) of the request being processed.
     *
     * This is useful if, just judging by the header,
     * you know you will not be able to process the message
     * for one reason or another (perhaps only temporarily).
     */
    Control RejectContent();

    /**
     * Sends a cached response back to the client.
     *
     * The response would have been previously cached
     * by a call to Responder::CacheAs(), after configuring
     * its various properties.
     */
    Control SendResponse(std::shared_ptr<CachedResponse>);

    /**
     * Sends a response back to the client.
     *
     * The actual response can be customized by configuring
     * the responder properties -- which you can access by
     * calling GetResponder().
     */
    Control SendResponse(Status);

    /**
     * Sends a response back to the client and closes the channel.
     *
     * The actual response can be customized by configuring
     * the responder properties -- which you can access by
     * calling GetResponder().
     */
    Control SendFinalResponse(Status);

    /**
     * Returns true if reading is currently being throttled
     * on this channel.
     */
    bool IsReadThrottled() const;

    /**
     * Returns true if writing is currently being throttled
     * on this channel.
     */
    bool IsWriteThrottled() const;

    /**
     * Specifies a throttler to use for read operations
     * on this channel.
     *
     * This is particularly useful if the client intends
     * to send a very big request body, or perhaps makes
     * a lot of requests on this particular channel.
     */
    void ThrottleRead(Throttler);

    /**
     * Specifies a throttler to use for write operations
     * on this channel.
     *
     * This is particularly useful if you intend to send
     * the client a very big response body, or perhaps
     * engage in many back and forth request-response
     * sequences on this particular channel.
     */
    void ThrottleWrite(Throttler);

protected:
    /**
     * This function is called whenever the channel
     * has new data to process. This normally happens
     * when a new request has just been accepted and parsed,
     * but might also be called if, right after the request
     * header has been processed, the first call to Process()
     * requests that the rest of the message's body be retrieved.
     * Once it is retrieved, this function will be called a 2nd time.
     */
    Control Process() override = 0;
};

} // namespace Http
} // namespace Yam

