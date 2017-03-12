#include "Responder.h"

#include <algorithm>
#include <array>
#include <fmtlib/format.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <time.h>

namespace Yam {
namespace Http {

namespace {
#define UPTR_PAIR(a, b) \
    {{ static_cast<std::uintptr_t>(a), reinterpret_cast<std::uintptr_t>(b) }}

std::string HttpVersion = "HTTP/1.1";
std::array<std::array<std::uintptr_t, 2>, 39> HttpStatusStrings = {{
    UPTR_PAIR(Status::Continue, "100 Continue"),
    UPTR_PAIR(Status::SwitchingProtocols, "101 Switching Protocols"),
    UPTR_PAIR(Status::Ok, "200 OK"),
    UPTR_PAIR(Status::Created, "201 Created"),
    UPTR_PAIR(Status::Accepted, "202 Accepted"),
    UPTR_PAIR(Status::NonAuthoritativeInformation, "203 Non-Authoritative Information"),
    UPTR_PAIR(Status::NoContent, "204 No Content"),
    UPTR_PAIR(Status::ResetContent, "205 Reset Content"),
    UPTR_PAIR(Status::PartialContent, "206 Partial Content"),
    UPTR_PAIR(Status::MultipleChoices, "300 Multiple Choices"),
    UPTR_PAIR(Status::MovedPermanently, "301 Moved Permanently"),
    UPTR_PAIR(Status::Found, "302 Found"),
    UPTR_PAIR(Status::SeeOther, "303 See Other"),
    UPTR_PAIR(Status::NotModified, "304 Not Modified"),
    UPTR_PAIR(Status::UseProxy, "305 Use Proxy"),
    // 306 is reserved
    UPTR_PAIR(Status::TemporaryRedirect, "307 Temporary Redirect"),
    UPTR_PAIR(Status::BadRequest, "400 Bad Request"),
    UPTR_PAIR(Status::Unauthorized, "401 Unauthorized"),
    // 402 is reserved
    UPTR_PAIR(Status::Forbidden, "403 Forbidden"),
    UPTR_PAIR(Status::NotFound, "404 Not Found"),
    UPTR_PAIR(Status::MethodNotAllowed, "405 Method Not Allowed"),
    UPTR_PAIR(Status::NotAcceptable, "406 Not Acceptable"),
    UPTR_PAIR(Status::ProxyAuthenticationRequired, "407 Proxy Authentication Required"),
    UPTR_PAIR(Status::RequestTimeout, "408 Request Timeout"),
    UPTR_PAIR(Status::Conflict, "409 Conflict"),
    UPTR_PAIR(Status::Gone, "410 Gone"),
    UPTR_PAIR(Status::LengthRequired, "411 Length Required"),
    UPTR_PAIR(Status::PreconditionFailed, "412 Precondition Failed"),
    UPTR_PAIR(Status::RequestEntityTooLarge, "413 Request Entity TooLarge"),
    UPTR_PAIR(Status::RequestUriTooLong, "414 Request-URI Too Long"),
    UPTR_PAIR(Status::UnsupportedMediaType, "415 Unsupported Media Type"),
    UPTR_PAIR(Status::RequestedRangeNotSatisfiable, "416 Requested Range Not Satisfiable"),
    UPTR_PAIR(Status::ExpectationFailed, "417 Expectation Failed"),
    UPTR_PAIR(Status::InternalServerError, "500 Internal Server Error"),
    UPTR_PAIR(Status::NotImplemented, "501 Not Implemented"),
    UPTR_PAIR(Status::BadGateway, "502 Bad Gateway"),
    UPTR_PAIR(Status::ServiceUnavailable, "503 Service Unavailable"),
    UPTR_PAIR(Status::GatewayTimeout, "504 Gateway Timeout"),
    UPTR_PAIR(Status::HttpVersionNotSupported, "505 Http Version Not Supported")
}};

const char* ToString(Status s) {
    auto&& pair = HttpStatusStrings.at(static_cast<std::uintptr_t>(s));
    return reinterpret_cast<const char*>(pair.at(1));
}

std::string CookieDate(const std::time_t& t) {
    char buffer[] = "Wdy, DD Mon YYYY HH:MM:SS GMT";
    struct tm tm;

    if (!::gmtime_r(&t, &tm))
        throw std::runtime_error("gmtime() failed");

    if(!std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %T GMT", &tm))
        throw std::runtime_error("Failed to format specified time");

    return buffer;
}

} // unnamed namespace

Responder::Responder(std::shared_ptr<OutputStream> stream) :
    _stream(std::move(stream)) {}

void Responder::Send(Status status) {
    fmt::MemoryWriter w;

    w.write("{} {}\r\n", HttpVersion, ToString(status));

    for (auto& nv : _fields)
        w.write("{}: {}\r\n", nv.first, nv.second);

    if (_body)
        w.write("Content-Length: {}\r\n", _body->size());

    w.write("\r\n");

    _header = w.str();

    _status = status;
}

std::pair<bool, std::size_t> Responder::Flush(std::size_t maxBytes) {
    std::size_t bytesWritten = 0;

    if (_writePosition < _header.size()) {
        auto quota = std::min(maxBytes, _header.size() - _writePosition);
        bytesWritten = _stream->Write(_header.c_str() + _writePosition, quota);

        _writePosition += bytesWritten;
        maxBytes -= bytesWritten;

        if (_writePosition != _header.size())
            return std::make_pair(false, bytesWritten);
    }

    if (_body) {
        auto bodyBytesConsumed = _writePosition - _header.size();
        auto quota = std::min(maxBytes, _body->size() - bodyBytesConsumed);
        bytesWritten = _stream->Write(_body->data() + bodyBytesConsumed, quota);

        _writePosition += bytesWritten;

        if (_writePosition - _header.size() != _body->size())
            return std::make_pair(false, bytesWritten);
    }

    return std::make_pair(true, bytesWritten);
}

bool Responder::GetKeepAlive() const {
    return _keepAlive;
}

void Responder::KeepAlive() {
    SetField("Connection", "keep-alive");
    _keepAlive = true;
}

void Responder::SetField(std::string name, std::string value) {
    _fields.emplace_back(std::move(name), std::move(value));
}

void Responder::SetCookie(std::string name, std::string value) {
    SetField("Set-Cookie", fmt::format("{}={}", std::move(name), std::move(value)));
}

void Responder::SetCookie(std::string name, std::string value, const CookieOptions& opts) {
    fmt::MemoryWriter w;
    std::string stringOpt;
    std::chrono::seconds durationOpt;
    std::time_t expirationOpt;

    if (opts.GetDomain(&stringOpt))
        w << "; Domain=" << stringOpt;

    if (opts.GetPath(&stringOpt))
        w << "; Path=" << stringOpt;

    if (opts.GetMaxAge(&durationOpt))
        w << "; Max-Age=" << durationOpt.count();

    if (opts.GetExpiration(&expirationOpt))
        w << "; Expires=" << CookieDate(expirationOpt);

    if (opts.IsHttpOnly())
        w << "; HttpOnly";

    if (opts.IsSecure())
        w << "; Secure";

    SetField("Set-Cookie", fmt::format("{}={}{}", std::move(name), std::move(value), w.str()));
}

void Responder::SetBody(std::shared_ptr<std::vector<char>> body) {
    _body = std::move(body);
}

Status Responder::GetStatus() const {
    return _status;
}

} // namespace Http
} // namespace Yam

