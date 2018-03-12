#pragma once

#include "InputStream.h"
#include "Parser.h"
#include "Protocol.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Nitra {

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
    std::string_view GetUri() const;

    /**
     * Gets the HTTP version of the request.
     */
    Version GetVersion() const;

    /**
     * Gets the names of all header fields present in the request.
     */
    std::vector<std::string_view> GetFieldNames() const;

    /**
     * Returns true if the request contains a field with the specified name.
     */
    bool HasField(const std::string_view& name) const;

    /**
     * Tries to get a field by name, returns true if it was found.
     *
     * If field was found and value is not null, then value is set.
     * Otherwise, value is ignored.
     */
    bool GetField(const std::string_view& name, std::string* value) const;

    /**
     * Gets a field by name.
     * Throws if the field does not exist.
     */
    std::string_view GetField(const std::string_view& name) const;

    /**
     * Gets the names of all cookies present in the request.
     */
    std::vector<std::string_view> GetCookieNames() const;

    /**
     * Gets a cookie by name.
     * Throws if the cookie does not exist.
     */
    std::string_view GetCookie(const std::string_view& name) const;

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
    bool ConsumeHeader(std::size_t maxBytes, std::size_t& totalBytesRead);

    /*
     * @internal
     * Read data from the request content body and advances the stream position.
     * Returns whether the opreation is completed, and how many bytes were read.
     */
    bool ConsumeContent(std::size_t maxBytes, std::size_t& totalBytesRead);

private:
    std::vector<char> _buffer;
    std::size_t _bufferPosition = 0;
    std::shared_ptr<InputStream> _input;
    Parser _parser;
    bool _onlySentHeaderFirst = false;
    std::vector<char> _content;
    std::size_t _contentPosition = 0;
};

} // namespace Nitra

