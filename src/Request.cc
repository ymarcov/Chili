#include "Request.h"
#include <strings.h>

namespace Yam {
namespace Http {

Request::Request(MemorySlot<Buffer> slot) :
    _slot{std::move(slot)},
    _parser{_slot.get(), sizeof(Buffer)} {
    _parser.Parse();
}

Protocol::Method Request::GetMethod() const {
    auto field = _parser.GetMethod();

    // indices must correspond to Protocol::Method enum
    auto methods = {
        "OPTIONS",
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "TRACE",
        "CONNECT"
    };

    int i = 0;
    for (auto& m : methods) {
        if (!::strncasecmp(field.Data, m, field.Size))
            return static_cast<Protocol::Method>(i);
        ++i;
    }

    throw std::runtime_error("Unknown HTTP method");
}

std::string Request::GetUri() const {
    auto f = _parser.GetUri();
    return {f.Data, f.Size};
}

Protocol::Version Request::GetProtocol() const {
    auto field = _parser.GetProtocolVersion();

    // indices must correspond to Protocol::Version
    auto versions = {
        "HTTP/1.0",
        "HTTP/1.1"
    };

    int i = 0;
    for (auto& v : versions) {
        if (!::strncasecmp(field.Data, v, field.Size))
            return static_cast<Protocol::Version>(i);
        ++i;
    }

    throw std::runtime_error("Unknown HTTP method");
}

std::string Request::GetField(const std::string& name) const {
    auto f = _parser.GetField(name);
    return {f.Data, f.Size};
}

std::string Request::GetCookie(const std::string& name) const {
    auto f = _parser.GetCookie(name);
    return {f.Data, f.Size};
}

std::vector<std::string> Request::GetCookieNames() const {
    std::vector<std::string> result;
    for (auto& f : _parser.GetCookieNames())
        result.emplace_back(f.Data, f.Size);
    return result;
}

std::pair<const char*, std::size_t> Request::GetBody() const {
    return {_parser.GetBody(), _parser.GetBodyLength()};
}


} // namespace Http
} // namespace Yam

