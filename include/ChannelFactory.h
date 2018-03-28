#pragma once

/**
 * @file ChannelFacotory.h
 */

#include "Channel.h"

namespace Chili {

/**
 * Creates channels for incoming sockets.
 * Channels are an HTTP state machine on top of an open socket.
 */
class ChannelFactory {
public:
    virtual ~ChannelFactory() = default;

    /**
     * Creates a new channel.
     *
     * @param fs           The FileStream (usually a socket) on top of which
     *                     the channel will carry out its I/O operations.
     * @param throttlers   The default throttlers to be used for this
     *                     particular channel. Setting them in the factory
     *                     is a good way to establish defaults among
     *                     all open channels.
     */
    virtual std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) = 0;
};

} // namespace Chili

