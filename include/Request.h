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

class Request {
public:
    using Buffer = char[8192];

    Request(MemorySlot<Buffer> emptyBuffer, std::shared_ptr<InputStream> input);

    /*
     * Gets the HTTP method of the request.
     */
    Method GetMethod() const;

    /*
     * Gets the URI string of the request.
     */
    std::string GetUri() const;

    /*
     * Gets the HTTP version of the request.
     */
    Version GetVersion() const;

    /*
     * Gets the names of all header fields present in the request.
     */
    std::vector<std::string> GetFieldNames() const;

    /*
     * Tries to get a field by name, returns true if it was found.
     *
     * If field was found and value is not null, then value is set.
     * Otherwise, value isn't ignored.
     *
     */
    bool GetField(const std::string& name, std::string* value) const;

    /*
     * Gets a field by name.
     * Throws if the field does not exist.
     */
    std::string GetField(const std::string& name) const;

    /*
     * Gets the names of all cookies present in the request.
     */
    std::vector<std::string> GetCookieNames() const;

    /*
     * Gets a cookie by name.
     * Throws if the cookie does not exist.
     */
    std::string GetCookie(const std::string& name) const;

    /*
     * Gets the length of the associated content.
     * Returns 0 if the Content-Length field is not present.
     */
    std::size_t GetContentLength() const;

    /*
     * Read data from the request body and advances the stream position.
     * Returns how many bytes were read.
     */
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

