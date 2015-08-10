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
    // reset out variable state

    result->_data = nullptr;
    result->_dataSize = 0;
    result->_headerSize = 0;
    std::memset(&result->_header, 0, sizeof(result->_header));

    // parse data

    auto charData = static_cast<const char*>(data.get());
    auto charDataSize = static_cast<int>(size);

    if (0 != ::h3_request_header_parse(&H(result->_header), charData, charDataSize))
        throw std::runtime_error("Failed to parse HTTP request");

    // store data/inc references in out variable

    result->_headerSize = GetHeaderSize(charData, charDataSize);
    result->_data = data;
    result->_dataSize = size;
}

std::size_t Request::GetHeaderSize(const char* dataPtr, std::size_t dataSize) {
    std::size_t headerSize = 0;

    // HTTP end of header is marked by an empty line.
    // Here we're iterating the data till we see a double
    // CRLF, which would mean that we're right at the end
    // of the line before the empty line.

    const std::size_t doubleCrlfLength = 4;

    while ( ((headerSize + doubleCrlfLength) < dataSize) &&
            std::strncmp("\r\n\r\n", dataPtr, doubleCrlfLength) ) {
        ++headerSize;
        ++dataPtr;
    }

    // put us right after the empty line.
    headerSize += doubleCrlfLength;

    if (headerSize > dataSize)
        throw std::runtime_error("Failed to find end of header");

    return headerSize;
}

Protocol::Method Request::GetMethod() const {
    static const char* methods[] = {
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
            return static_cast<Protocol::Method>(i);

    throw std::runtime_error("Invalid request method");
}

std::string Request::GetUri() const {
    return std::string(H(_header).RequestURI, H(_header).RequestURILen);
}

Protocol::Version Request::GetHttpVersion() const {
    static const char* versions[] = {
        "HTTP/1.0",
        "HTTP/1.1"
    };

    for (int i = 0; i < sizeof(versions) / sizeof(*versions); i++)
        if (!std::strncmp(versions[i], H(_header).HTTPVersion, H(_header).HTTPVersionLen))
            return static_cast<Protocol::Version>(i);

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

std::size_t Request::GetBodySize() const {
    return _dataSize - _headerSize;
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

