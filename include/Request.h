#pragma once

#include "Http.h"

#include <cstddef>
extern "C" {
#include <h3.h>
}
#include <memory>
#include <string>

namespace Yam {
namespace Http {

class Request {
public:
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

    static Request Parse(std::shared_ptr<const void> data, std::size_t size);

    Request();

    Method GetMethod() const;
    std::string GetUri() const;
    HttpVersion GetHttpVersion() const;
    std::string GetUserAgent() const;
    std::string GetAccept() const;
    const char* GetBody() const;

private:
    static std::size_t GetHeaderSize(const char* data, std::size_t size);
    std::string GetField(const char*) const;

    RequestHeader _header; // h3 data type
    std::shared_ptr<const void> _data;
    std::size_t _dataSize;
    std::size_t _headerSize;
};

} // namespace Http
} // namespace Yam

