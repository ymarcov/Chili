#pragma once

#include "InputStream.h"
#include "Parser.h"
#include "Protocol.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Yam {
namespace Http {

class Request {
public:
    using Buffer = char[8192];

    Request() = default;
    Request(std::shared_ptr<void> emptyBuffer, std::shared_ptr<InputStream> input);

    /*
     * Reads and parses the request.
     * Returns whether the opreation is completed, and how many bytes were read.
     */
    std::pair<bool, std::size_t> ConsumeHeader(std::size_t maxBytes);

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
     * Otherwise, value is ignored.
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
     * Gets whether the connection should be kept alive.
     */
    bool KeepAlive() const;

    /*
     * Read data from the request body and advances the stream position.
     * Returns whether the opreation is completed, and how many bytes were read.
     */
    std::pair<bool, std::size_t> ConsumeBody(void* buffer, std::size_t bufferSize);

private:
    std::shared_ptr<void> _buffer;
    std::size_t _bufferPosition = 0;
    std::shared_ptr<InputStream> _input;
    Parser _parser;
    std::size_t _contentBytesReadFromInitialBuffer = 0;
    bool _onlySentHeaderFirst = false;
};

} // namespace Http
} // namespace Yam

