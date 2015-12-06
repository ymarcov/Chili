#pragma once

#include "InputStream.h"
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

    /*
     * Reads data from input into the specified buffer, parses it,
     * and keeps retrieving data from it as necessary according
     * to the length of the request body.
     *
     * Mostly useful for POST and PUT operations where the request
     * body can be quite large.
     *
     * Note: The whole header itself (excluding the body)
     * must still not be bigger than the buffer size.
     */
    Request(MemorySlot<Buffer> emptyBuffer, std::shared_ptr<InputStream> input);

    Protocol::Method GetMethod() const;
    std::string GetUri() const;
    Protocol::Version GetProtocol() const;
    std::vector<std::string> GetFieldNames() const;
    std::string GetField(const std::string& name) const;
    std::string GetCookie(const std::string& name) const;
    std::vector<std::string> GetCookieNames() const;
    std::size_t GetContentLength() const;
    std::size_t ReadNextBodyChunk(void* buffer, std::size_t bufferSize);

private:
    MemorySlot<Buffer> _buffer;
    std::shared_ptr<InputStream> _input;
    Parser _parser;
    std::size_t _contentBytesReadFromInitialBuffer = 0;
    bool _onlySentHeaderFirst = false;
};

} // namespace Http
} // namespace Yam

