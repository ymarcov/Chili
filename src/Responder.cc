#include "Responder.h"

#include <array>
#include <cppformat/format.h>
#include <string>

namespace Yam {
namespace Http {

namespace {
#define UPTR_PAIR(a, b) \
    { static_cast<std::uintptr_t>(a), reinterpret_cast<std::uintptr_t>(b) }

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

    _stream->Write(w.c_str(), w.size());

    if (_body)
        _stream->Write(_body->data(), _body->size());
}

void Responder::SetField(std::string name, std::string value) {
    _fields.emplace_back(std::move(name), std::move(value));
}

void Responder::SetBody(std::shared_ptr<std::vector<char>> body) {
    _body = std::move(body);
}

} // namespace Http
} // namespace Yam

