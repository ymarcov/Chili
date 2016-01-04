#pragma once

#include <chrono>
#include <ctime>
#include <string>
#include <utility>

namespace Yam {
namespace Http {
inline namespace Protocol {

enum class Method {
    Options,
    Get,
    Head,
    Post,
    Put,
    Delete,
    Trace,
    Connect
};

enum class Version {
    Http10,
    Http11
};

enum class Status {
    Continue, // 100
    SwitchingProtocols, // 101
    Ok, // 200
    Created, // 201
    Accepted, // 202
    NonAuthoritativeInformation, // 203
    NoContent, // 204
    ResetContent, // 205
    PartialContent, // 206
    MultipleChoices, // 300
    MovedPermanently, // 301
    Found, // 302
    SeeOther, // 303
    NotModified, // 304
    UseProxy, // 305
    // 306 is reserved
    TemporaryRedirect, // 307
    BadRequest, // 400
    Unauthorized, // 401
    // 402 is reserved
    Forbidden, // 403
    NotFound, // 404
    MethodNotAllowed, // 405
    NotAcceptable, // 406
    ProxyAuthenticationRequired, // 407
    RequestTimeout, // 408
    Conflict, // 409
    Gone, // 410
    LengthRequired, // 411
    PreconditionFailed, // 412
    RequestEntityTooLarge, // 413
    RequestUriTooLong, // 414
    UnsupportedMediaType, // 415
    RequestedRangeNotSatisfiable, // 416
    ExpectationFailed, // 417
    InternalServerError, // 500
    NotImplemented, // 501
    BadGateway, // 502
    ServiceUnavailable, // 503
    GatewayTimeout, // 504
    HttpVersionNotSupported // 505
};

class CookieOptions {
public:
    void SetDomain(const std::string&);
    void SetPath(const std::string&);
    void SetMaxAge(const std::chrono::seconds&);
    void SetExpiration(const std::time_t&);

    bool GetDomain(std::string*) const;
    bool GetPath(std::string*) const;
    bool GetMaxAge(std::chrono::seconds*) const;
    bool GetExpiration(std::time_t*) const;

private:
    std::pair<bool, std::string> _domain;
    std::pair<bool, std::string> _path;
    std::pair<bool, std::chrono::seconds> _maxAge;
    std::pair<bool, std::time_t> _expiration;
};

} // namespace Protocol
} // namespace Http
} // namespace Yam

