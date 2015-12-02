#pragma once

#include "MemoryPool.h"
#include "Parser.h"
#include "Protocol.h"

#include <string>
#include <utility>
#include <vector>

namespace Yam {
namespace Http {

/*
 * Taking ownership of a request buffer, provides
 * a simple API to access its various properties.
 */
class Request {
public:
    using Buffer = char[8192];

    Request(MemorySlot<Buffer> slot);

    Protocol::Method GetMethod() const;
    std::string GetUri() const;
    Protocol::Version GetProtocol() const;
    std::string GetField(const std::string& name) const;
    std::string GetCookie(const std::string& name) const;
    std::vector<std::string> GetCookieNames() const;
    std::pair<const char*, std::size_t> GetBody() const;

private:
    MemorySlot<Buffer> _slot;
    Parser _parser;
};

} // namespace Http
} // namespace Yam

