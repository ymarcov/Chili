#include "Router.h"

namespace Nitra {

class RoutedChannel : public Channel {
public:
    RoutedChannel(std::shared_ptr<FileStream> fs, std::shared_ptr<Router> router);

    Control Process(const Request&, Response&) override;

private:
    std::shared_ptr<Router> _router;
};

Router::Router() {
    _defaultHandler = [](auto& channel, auto& args) {
        return Status::NotFound;
    };
}

Channel::Control Router::InvokeRoute(Channel& channel) const {
    const RouteHandler* handler;
    Args args;

    if (FindMatch(channel.GetRequest(), handler, args)) {
        auto status = (*handler)(channel, args);
        return channel.SendResponse(status);
    } else {
        auto status = _defaultHandler(channel, args);
        return channel.SendFinalResponse(status);
    }
}

bool Router::FindMatch(const Request& request, const RouteHandler*& outHandler, Args& outArgs) const {
    auto method = static_cast<int>(request.GetMethod());
    auto uri = std::string(request.GetUri());

    auto methodRoutes = _routes.find(method);

    if (methodRoutes != end(_routes)) {
        auto& routes = methodRoutes->second;

        for (auto& route : routes) {
            auto& regex = route.first;
            auto& handler = route.second;
            std::smatch matches;

            if (std::regex_match(uri, matches, regex)) {
                outArgs.reserve(matches.size() - 1);

                for (auto i = begin(matches) + 1, e = end(matches); i != e; ++i)
                    outArgs.push_back(std::move(*i));

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

RoutedChannel::RoutedChannel(std::shared_ptr<FileStream> fs, std::shared_ptr<Router> router) :
    Channel(std::move(fs)),
    _router(std::move(router)) {}

Channel::Control RoutedChannel::Process(const Request&, Response&) {
    return _router->InvokeRoute(*this);
}

RoutedChannelFactory::RoutedChannelFactory(std::shared_ptr<Router> router) :
    _router(std::move(router)) {}

std::unique_ptr<Channel> RoutedChannelFactory::CreateChannel(std::shared_ptr<FileStream> fs) {
    return std::make_unique<RoutedChannel>(std::move(fs), _router);
}

} // namespace Nitra

