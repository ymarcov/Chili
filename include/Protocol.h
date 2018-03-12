#pragma once

#include <chrono>
#include <ctime>
#include <string>
#include <utility>

namespace Nitra {

/**
 * Test
 */
inline namespace Protocol {

/**
 * HTTP Protocol related enums and classes.
 */
enum class Method {
    Options, ///< OPTIONS
    Get, ///< GET
    Head, ///< HEAD
    Post, ///< POST
    Put, ///< PUT
    Delete, ///< DELETE
    Trace, ///< TRACE
    Connect ///< CONNECT
};

/**
 * An HTTP request/response version.
 */
enum class Version {
    Http10, ///< HTTP/1.0
    Http11 ///< HTTP/1.1
};

/**
 * An HTTP response status.
 */
enum class Status {
    Continue, ///< 100
    SwitchingProtocols, ///< 101
    Ok, ///< 200
    Created, ///< 201
    Accepted, ///< 202
    NonAuthoritativeInformation, ///< 203
    NoContent, ///< 204
    ResetContent, ///< 205
    PartialContent, ///< 206
    MultipleChoices, ///< 300
    MovedPermanently, ///< 301
    Found, ///< 302
    SeeOther, ///< 303
    NotModified, ///< 304
    UseProxy, ///< 305
    // 306 is reserved
    TemporaryRedirect, ///< 307
    BadRequest, ///< 400
    Unauthorized, ///< 401
    // 402 is reserved
    Forbidden, ///< 403
    NotFound, ///< 404
    MethodNotAllowed, ///< 405
    NotAcceptable, ///< 406
    ProxyAuthenticationRequired, ///< 407
    RequestTimeout, ///< 408
    Conflict, ///< 409
    Gone, ///< 410
    LengthRequired, ///< 411
    PreconditionFailed, ///< 412
    RequestEntityTooLarge, ///< 413
    RequestUriTooLong, ///< 414
    UnsupportedMediaType, ///< 415
    RequestedRangeNotSatisfiable, ///< 416
    ExpectationFailed, ///< 417
    InternalServerError, ///< 500
    NotImplemented, ///< 501
    BadGateway, ///< 502
    ServiceUnavailable, ///< 503
    GatewayTimeout, ///< 504
    HttpVersionNotSupported ///< 505
};

/**
 * Request/Response Transfer mode.
 */
enum class TransferMode {
    Normal, ///< Sends entire content in one chunk
    Chunked ///< Sends content in chunks
};

/**
 * Content compression type.
 */
enum class Compression {
    None, ///< No compression
    Deflate, ///< Compress using the deflate algorithm
    Gzip ///< Compress using the gzip algorithm
};

/**
 * Extra options to specify for cookies.
 */
class CookieOptions {
public:
    /**
     * Sets the cookie's domain.
     */
    void SetDomain(const std::string&);

    /**
     * Sets the cookie's path.
     */
    void SetPath(const std::string&);

    /**
     * Sets the max age of the cookie.
     */
    void SetMaxAge(const std::chrono::seconds&);

    /**
     * Sets the expiration time of the cookie.
     */
    void SetExpiration(const std::time_t&);

    /**
     * Sets whether the cookie is to be accessed
     * only by the server, and not from client side scripts.
     *
     * (This is provided here, but its use in server code is questionable)
     */
    void SetHttpOnly(bool = true);

    /**
     * Sets whether the cookie may only be sent
     * over HTTPS connections.
     */
    void SetSecure(bool = true);

    /**
     * Gets the cookie's domain.
     */
    bool GetDomain(std::string*) const;

    /**
     * Gets the cookie's path.
     */
    bool GetPath(std::string*) const;

    /**
     * Gets the max age of the cookie.
     */
    bool GetMaxAge(std::chrono::seconds*) const;

    /**
     * Gets the cookie's expiration time.
     */
    bool GetExpiration(std::time_t*) const;

    /**
     * Gets whether the cookie may only be
     * accessed by the server.
     */
    bool IsHttpOnly() const;

    /**
     * Gets whether the cookie may only be sent
     * over HTTPS connections.
     */
    bool IsSecure() const;

private:
    std::pair<bool, std::string> _domain;
    std::pair<bool, std::string> _path;
    std::pair<bool, std::chrono::seconds> _maxAge;
    std::pair<bool, std::time_t> _expiration;
    bool _httpOnly = false;
    bool _secure = false;
};

} // namespace Protocol
} // namespace Nitra

