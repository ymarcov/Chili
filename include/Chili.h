#pragma once

#include "ChannelFactory.h"
#include "HttpServer.h"
#include "Log.h"

#include <functional>

namespace Chili {

auto RequestHandler(std::function<void(Channel&)> handler) {
    return ChannelFactory::Create(handler);
}

} // namespace Chili

