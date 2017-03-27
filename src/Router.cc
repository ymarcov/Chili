#include "Router.h"

namespace Yam {
namespace Http {

Router::Router() {
    _defaultHandler = [](auto& channel, auto& params) {
        return Status::NotFound;
    };
}

Channel::Control Router::InvokeRoute(Channel& channel) const {
    const RouteHandler* handler;
    Params params;

    if (FindMatch(channel.GetRequest(), handler, params)) {
        auto status = (*handler)(channel, params);
        return channel.SendResponse(status);
    } else {
        auto status = _defaultHandler(channel, params);
        return channel.SendFinalResponse(status);
    }
}

bool Router::FindMatch(const Request& request, const RouteHandler*& outHandler, Params& outParams) const {
    auto method = static_cast<int>(request.GetMethod());
    auto uri = std::string(request.GetUri());

    auto methodRoutes = _routes.find(method);

    if (methodRoutes != end(_routes)) {
        auto& routes = methodRoutes->second;

        for (auto& route : methodRoutes->second) {
            auto& regex = route.first;
            auto& handler = route.second;
            std::smatch matches;

            if (std::regex_match(uri, matches, regex)) {
                outParams.reserve(matches.size() - 1);

                for (auto i = begin(matches) + 1, e = end(matches); i != e; ++i)
                    outParams.push_back(std::move(*i));

                outHandler = &handler;

                return true;

            }
        }
    }

    return false;
}

void Router::InstallRoute(Method method, std::string pattern, RouteHandler handler) {
    _routes[static_cast<int>(method)].push_back(std::make_pair(std::regex(pattern), std::move(handler)));
}

void Router::InstallDefault(RouteHandler handler) {
    _defaultHandler = std::move(handler);
}

} // namespace Http
} // namespace Yam

