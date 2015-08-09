#pragma once

#include "Protocol.h"

#include <cstddef>
#include <memory>
#include <string>

namespace Yam {
namespace Http {

class Request {
public:

    static void Parse(Request* out, std::shared_ptr<const void> data, std::size_t size);

    Protocol::Method GetMethod() const;
    std::string GetUri() const;
    Protocol::Version GetHttpVersion() const;
    std::string GetUserAgent() const;
    std::string GetAccept() const;
    const char* GetBody() const;

private:
    static std::size_t GetHeaderSize(const char* data, std::size_t size);
    std::string GetField(const char*) const;

    char _header[0x300];
    std::shared_ptr<const void> _data;
    std::size_t _dataSize;
    std::size_t _headerSize;
};

} // namespace Http
} // namespace Yam

