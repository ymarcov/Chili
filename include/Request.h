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

/**
 * An HTTP request.
 */
class Request {
public:
    Request() = default;
    Request(std::shared_ptr<InputStream> input);

    /**
     * Gets the HTTP method of the request.
     */
    Method GetMethod() const;

    /**
     * Gets the URI string of the request.
     */
    std::string GetUri() const;

    /**
     * Gets the HTTP version of the request.
     */
    Version GetVersion() const;

    /**
     * Gets the names of all header fields present in the request.
     */
    std::vector<std::string> GetFieldNames() const;

    /**
     * Tries to get a field by name, returns true if it was found.
     *
     * If field was found and value is not null, then value is set.
     * Otherwise, value is ignored.
     */
    bool GetField(const std::string& name, std::string* value) const;

    /**
     * Gets a field by name.
     * Throws if the field does not exist.
     */
    std::string GetField(const std::string& name) const;

    /**
     * Gets the names of all cookies present in the request.
     */
    std::vector<std::string> GetCookieNames() const;

    /**
     * Gets a cookie by name.
     * Throws if the cookie does not exist.
     */
    std::string GetCookie(const std::string& name) const;

    /**
     * Returns true if the request has a message
     * body (i.e. Content-Length > 0).
     */
    bool HasContent() const;

    /**
     * If the message has associated content, returns true if
     * the content is already entirely available to be read.
     */
    bool IsContentAvailable() const;

    /**
     * Gets the length of the associated content.
     * Returns 0 if the Content-Length field is not present.
     */
    std::size_t GetContentLength() const;

    /**
     * Gets whether the connection should be kept alive.
     */
    bool KeepAlive() const;

    /**
     * Gets the message content, beside the header.
     */
    const std::vector<char>& GetContent() const;

    /**
     * @internal
     * Reads and parses the request.
     * Returns whether the opreation is completed, and how many bytes were read.
     */
    std::pair<bool, std::size_t> ConsumeHeader(std::size_t maxBytes);

    /*
     * @internal
     * Read data from the request content body and advances the stream position.
     * Returns whether the opreation is completed, and how many bytes were read.
     */
    std::pair<bool, std::size_t> ConsumeContent(std::size_t maxBytes);

private:
    std::vector<char> _buffer;
    std::size_t _bufferPosition = 0;
    std::shared_ptr<InputStream> _input;
    Parser _parser;
    std::size_t _contentBytesReadFromInitialBuffer = 0;
    bool _onlySentHeaderFirst = false;
    std::vector<char> _content;
    std::size_t _contentPosition = 0;
};

} // namespace Http
} // namespace Yam

