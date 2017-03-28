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

/**
 * A request router, mapping requests containing a
 * certain HTTP method along with a certain URI
 * to associated handler functions.
 */
class Router {
public:
    /**
     * The set of URI parameters associated with the route.
     *
     * For example, if your route was "/hello/(.+)", then
     * Args[0] will contain whatever was captured in the group.
     */
    using Args = std::vector<std::string>;

    /**
     * A function that, upon receiving a new request,
     * will handle it.
     *
     * @param channel The channel which got the request
     * @param args    The arguments for the specified route
     */
    using RouteHandler = std::function<Status(Channel& channel, const Args& args)>;

    Router();
    virtual ~Router() = default;

    /**
     * @internal
     */
    Channel::Control InvokeRoute(Channel&) const;

    /**
     * Installs a new route handler for the specified method and URI pattern.
     *
     * @param method  The method to operate on
     * @param pattern The regular-expression URI pattern to match
     * @param handler The handler function to run in the case of a match
     */
    void InstallRoute(Method method, std::string pattern, RouteHandler handler);

    /**
     * Installs a new route handler for cases where no installed route was matched.
     * This is particularly useful for serving 404 pages.
     *
     * @param handler The handler function to run when no
     *                route matches a given request.
     */
    void InstallDefault(RouteHandler handler);

private:
    bool FindMatch(const Request&, const RouteHandler*&, Args&) const;

    std::unordered_map<int, std::vector<std::pair<std::regex, RouteHandler>>> _routes;
    RouteHandler _defaultHandler;
};

/**
 * A factory for creating routed channels.
 */
class RoutedChannelFactory : public ChannelFactory {
public:
    /**
     * Creates a new routed channel factory which creates
     * new channels that employ the specified router
     * for handling incoming requests.
     */
    RoutedChannelFactory(std::shared_ptr<Router> router);

    std::unique_ptr<Channel> CreateChannel(std::shared_ptr<FileStream> fs) override;

private:
    std::shared_ptr<Router> _router;
};

} // namespace Http
} // namespace Yam

