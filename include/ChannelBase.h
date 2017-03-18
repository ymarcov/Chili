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

    Request& GetRequest();
    Responder& GetResponder();

    Control FetchContent();
    Control RejectContent();
    Control SendResponse(std::shared_ptr<CachedResponse>);
    Control SendResponse(Status);

    bool IsReadThrottled() const;
    bool IsWriteThrottled() const;
    void ThrottleRead(Throttler);
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

