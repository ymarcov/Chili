#include "Request.h"

extern "C" {
#include <h3.h>
}

#include <cstring>
#include <strings.h>

namespace Yam {
namespace Http {

namespace {
RequestHeader& H(char* buf) { return reinterpret_cast<RequestHeader&>(*buf); }
const RequestHeader& H(const char* buf) { return reinterpret_cast<const RequestHeader&>(*buf); }
} // unnamed namespace

void Request::Parse(Request* result, std::shared_ptr<const void> data, std::size_t size) {
    auto charData = static_cast<const char*>(data.get());
    auto charDataSize = static_cast<int>(size);

    if (0 != h3_request_header_parse(&H(result->_header), charData, charDataSize))
        throw std::runtime_error("Failed to parse HTTP request");

    result->_data = data;
    result->_dataSize = size;
    result->_headerSize = GetHeaderSize(charData, charDataSize);
}

std::size_t Request::GetHeaderSize(const char* data, std::size_t size) {
    std::size_t result = 0;

    while (result < size && std::strncmp("\r\n\r\n", data, 4)) {
        ++result;
        ++data;
    }

    if (result >= size)
        throw std::runtime_error("Failed to find end of header");

    return result + 4;
}

Request::Request() {
    std::memset(&_header, 0, sizeof(_header));
}

Request::Method Request::GetMethod() const {
    const char* methods[] = {
        "OPTIONS",
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "TRACE",
        "CONNECT"
    };

    for (int i = 0; i < sizeof(methods) / sizeof(*methods); i++)
        if (!std::strncmp(methods[i], H(_header).RequestMethod, H(_header).RequestMethodLen))
            return static_cast<Method>(i);

    throw std::runtime_error("Invalid request method");
}

std::string Request::GetUri() const {
    return std::string(H(_header).RequestURI, H(_header).RequestURILen);
}

HttpVersion Request::GetHttpVersion() const {
    const char* versions[] = {
        "HTTP/1.0",
        "HTTP/1.1"
    };

    for (int i = 0; i < sizeof(versions) / sizeof(*versions); i++)
        if (!std::strncmp(versions[i], H(_header).HTTPVersion, H(_header).HTTPVersionLen))
            return static_cast<HttpVersion>(i);

    throw std::runtime_error("Unsupported request HTTP version");
}

std::string Request::GetUserAgent() const {
    return GetField("user-agent");
}

std::string Request::GetAccept() const {
    return GetField("accept");
}

const char* Request::GetBody() const {
    return static_cast<const char*>(_data.get()) + _headerSize;
}

std::string Request::GetField(const char* field) const {
    for (int i = 0; i < H(_header).HeaderSize; i++) {
        auto f = H(_header).Fields[i];
        if (!::strncasecmp(field, f.FieldName, f.FieldNameLen))
            return std::string(f.Value, f.ValueLen);
    }

    return std::string();
}

} // namespace Http
} // namespace Yam
