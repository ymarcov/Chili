#pragma once

#include "ChannelBase.h"

namespace Yam {
namespace Http {

class ChannelFactory {
public:
    virtual ~ChannelFactory() = default;

    virtual std::unique_ptr<ChannelBase> CreateChannel(std::shared_ptr<FileStream>, Channel::Throttlers) = 0;
};

} // namespace Http
} // namespace Yam

