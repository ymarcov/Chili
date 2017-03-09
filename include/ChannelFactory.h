#pragma once

#include "Channel.h"

namespace Yam {
namespace Http {

class ChannelFactory {
public:
    virtual ~ChannelFactory() = default;

    virtual std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream>, Channel::Throttlers) = 0;
};

} // namespace Http
} // namespace Yam

