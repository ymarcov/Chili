#pragma once

#include "Parser.h"
#include "MemoryPool.h"

#include <string>
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

    std::string GetMethod() const {
        auto f = _parser.GetMethod();
        return {f.Data, f.Size};
    }

    std::string GetUri() const {
        auto f = _parser.GetUri();
        return {f.Data, f.Size};
    }

    std::string GetProtocol() const {
        auto f = _parser.GetProtocolVersion();
        return {f.Data, f.Size};
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

