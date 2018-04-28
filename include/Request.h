#pragma once

#include "InputStream.h"
#include "Protocol.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Chili {

/**
 * An HTTP request.
 */
class Request {
public:
    Request() = default;
    Request(std::shared_ptr<InputStream> input);
    ~Request();

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
     * Gets the names of all headers present in the request.
     */
    std::vector<std::string_view> GetHeaderNames() const;

    /**
     * Returns true if the request contains a header with the specified name.
     */
    bool HasHeader(const std::string_view& name) const;

    /**
     * Tries to get a header by name, returns true if it was found.
     *
     * If header was found and value is not null, then value is set.
     * Otherwise, value is ignored.
     */
    bool GetHeader(const std::string_view& name, std::string* value) const;

    /**
     * Gets a header by name.
     * Throws if the header does not exist.
     */
    std::string_view GetHeader(const std::string_view& name) const;

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
     * Returns 0 if the Content-Length header is not present.
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
    friend class HttpParserSettings;

    class HttpParserStringBuilder& GetStringBuilder();

    std::vector<char> _buffer;
    std::size_t _bufferPosition = 0;
    std::shared_ptr<InputStream> _input;
    void* _httpParser = nullptr;
    void* _httpParserStringBuilder = nullptr;
    std::size_t _headerBytesParsed = 0;
    bool _parsedHeader = false;
    bool _onlySentHeaderFirst = false;
    std::string_view _uri;
    std::vector<std::pair<std::string_view, std::string_view>> _headers;
    std::vector<char> _content;
    std::size_t _contentPosition = 0;
};

} // namespace Chili

