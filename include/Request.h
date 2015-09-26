#pragma once

#include "MemoryPool.h"
#include "Parser.h"
#include "Protocol.h"

#include <string>
#include <strings.h>
#include <utility>
#include <vector>

namespace Yam {
namespace Http {

class Request {
public:
    using Buffer = char[8192];

    Request(MemorySlot<Buffer> slot) :
        _slot{std::move(slot)},
        _parser{_slot.get(), sizeof(Buffer)} {
        _parser.Parse();
    }

    Protocol::Method GetMethod() const {
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

    std::string GetUri() const {
        auto f = _parser.GetUri();
        return {f.Data, f.Size};
    }

    Protocol::Version GetProtocol() const {
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

    std::string GetField(const std::string& name) const {
        auto f = _parser.GetField(name);
        return {f.Data, f.Size};
    }

    std::string GetCookie(const std::string& name) const {
        auto f = _parser.GetCookie(name);
        return {f.Data, f.Size};
    }

    std::vector<std::string> GetCookieNames() const {
        std::vector<std::string> result;
        for (auto& f : _parser.GetCookieNames())
            result.emplace_back(f.Data, f.Size);
        return result;
    }

    std::pair<const char*, std::size_t> GetBody() const {
        return {_parser.GetBody(), _parser.GetBodyLength()};
    }

private:
    MemorySlot<Buffer> _slot;
    Parser _parser;
};

} // namespace Http
} // namespace Yam

