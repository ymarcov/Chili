#include "Router.h"

namespace Chili {

class RoutedChannel : public Channel {
public:
    RoutedChannel(std::shared_ptr<FileStream> fs, std::shared_ptr<Router> router);

    void Process() override;

private:
    std::shared_ptr<Router> _router;
};

Router::Router() {
    _defaultHandler = [](auto& channel, auto& args) {
        return Status::NotFound;
    };
}

void Router::InvokeRoute(Channel& channel) const {
    const RouteHandler* handler;
    Args args;
    Protocol::Status status;

    if (FindMatch(channel.GetRequest(), handler, args)) {
        status = (*handler)(channel, args);
    } else {
        status = _defaultHandler(channel, args);
        channel.GetResponse().CloseConnection();
    }

    channel.GetResponse().SetStatus(status);
    channel.SendResponse();
}

bool Router::FindMatch(const Request& request, const RouteHandler*& outHandler, Args& outArgs) const {
    auto method = static_cast<int>(request.GetMethod());
    auto uri = std::string(request.GetUri());

    auto methodRoutes = _routes.find(method);

    if (methodRoutes != end(_routes)) {
        auto& routes = methodRoutes->second;

        for (auto& route : routes) {
            auto& [regex, handler] = route;
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

void RoutedChannel::Process() {
    _router->InvokeRoute(*this);
}

RoutedChannelFactory::RoutedChannelFactory(std::shared_ptr<Router> router) :
    _router(std::move(router)) {}

std::shared_ptr<Channel> RoutedChannelFactory::CreateChannel(std::shared_ptr<FileStream> fs) {
    return std::make_shared<RoutedChannel>(std::move(fs), _router);
}

} // namespace Chili

