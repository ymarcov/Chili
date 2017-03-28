#pragma once

#include "Channel.h"
#include "ChannelFactory.h"
#include "Protocol.h"

#include <functional>
#include <map>
#include <regex>
#include <unordered_map>

namespace Yam {
namespace Http {

class Router {
public:
    using Params = std::vector<std::string>;
    using RouteHandler = std::function<Status(Channel& channel, const Params& params)>;

    Router();
    virtual ~Router() = default;

    Channel::Control InvokeRoute(Channel&) const;
    void InstallRoute(Method method, std::string pattern, RouteHandler handler);
    void InstallDefault(RouteHandler handler);

private:
    bool FindMatch(const Request&, const RouteHandler*&, Params&) const;

    std::unordered_map<int, std::vector<std::pair<std::regex, RouteHandler>>> _routes;
    RouteHandler _defaultHandler;
};

class RoutedChannel : public Channel {
public:
    RoutedChannel(std::shared_ptr<FileStream> fs, std::shared_ptr<Router> router);

    Control Process(const Request&, Response&) override;

private:
    std::shared_ptr<Router> _router;
};

class RoutedChannelFactory : public ChannelFactory {
public:
    RoutedChannelFactory(std::shared_ptr<Router> router);

    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override;

private:
    std::shared_ptr<Router> _router;
};

} // namespace Http
} // namespace Yam

