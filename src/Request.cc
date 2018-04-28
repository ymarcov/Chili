#include "Request.h"
#include "SystemError.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <http-parser/http_parser.h>
#include <strings.h>

namespace Chili {

constexpr std::size_t BufferSize = 0x2000;

class HttpParserStringBuilder {
public:
    void Reset(const char* at) {
        Data = at;
        Length = 0;
        State = 0;
    }

    void Feed(std::size_t length) {
        Length += length;
    }

    const char* Data = nullptr;
    std::size_t Length = 0;
    int State = 0;
};

static ::http_parser* GetParser(void* ptr) {
    return static_cast<::http_parser*>(ptr);
}

static class HttpParserSettings {
public:
    HttpParserSettings() {
        _settings.on_url = [](auto parser, auto at, auto length) {
            if (!length)
                return 0;

            auto& r = GetRequest(parser);
            auto& sb = r.GetStringBuilder();

            if (sb.State != 1) {
                sb.Reset(at);
                sb.State = 1;
            }

            sb.Feed(length);

            r._uri = std::string_view(sb.Data, sb.Length);

            return 0;
        };

        _settings.on_header_field = [](auto parser, auto at, auto length) {
            if (!length)
                return 0;

            auto& r = GetRequest(parser);
            auto& sb = r.GetStringBuilder();

            if (sb.State != 2) {
                r._headers.push_back({});
                sb.Reset(at);
                sb.State = 2;
            }

            sb.Feed(length);

            r._headers.back().first = std::string_view(sb.Data, sb.Length);

            return 0;
        };

        _settings.on_header_value = [](auto parser, auto at, auto length) {
            if (!length)
                return 0;

            auto& r = GetRequest(parser);
            auto& sb = r.GetStringBuilder();

            if (sb.State != 3) {
                sb.Reset(at);
                sb.State = 3;
            }

            sb.Feed(length);

            r._headers.back().second = std::string_view(sb.Data, sb.Length);

            return 0;
        };

        _settings.on_headers_complete = [](auto parser) {
            GetRequest(parser)._parsedHeader = true;
            return 2; // don't parse body for us
        };
    }

    operator const http_parser_settings*() const {
        return &_settings;
    }

private:
    static Request& GetRequest(::http_parser* parser) {
        return *static_cast<Request*>(parser->data);
    }

    ::http_parser_settings _settings;
} httpParserSettings;

Request::Request(std::shared_ptr<InputStream> input) :
    _buffer(BufferSize),
    _input{std::move(input)} {
    auto parser = std::make_unique<::http_parser>();
    ::http_parser_init(parser.get(), HTTP_REQUEST);
    parser->data = this;
    _httpParser = parser.release();
    _httpParserStringBuilder = new HttpParserStringBuilder();
    _headers.reserve(8);
}

Request::~Request() {
    delete GetParser(_httpParser);
    delete &GetStringBuilder();
}

HttpParserStringBuilder& Request::GetStringBuilder() {
    return *static_cast<HttpParserStringBuilder*>(_httpParserStringBuilder);
}

bool Request::ConsumeHeader(std::size_t maxBytes, std::size_t& bytesRead) {
    if (_bufferPosition == 0)
        std::memset(_buffer.data(), 0, BufferSize);

    auto quota = std::min(maxBytes, BufferSize - _bufferPosition);

    bytesRead = _input->Read(_buffer.data() + _bufferPosition, quota);

    if (!bytesRead)
        return false;

    auto parser = GetParser(_httpParser);

    auto bytesParsed = ::http_parser_execute(parser,
                                             httpParserSettings,
                                             _buffer.data() + _bufferPosition,
                                             bytesRead);

    _bufferPosition += bytesRead;
    _headerBytesParsed += bytesParsed;

    if (!_parsedHeader) {
        if (bytesParsed != bytesRead)
            throw std::runtime_error(::http_errno_name(static_cast<::http_errno>(parser->http_errno)));
        else if (_bufferPosition == BufferSize)
            throw std::runtime_error("No end-of-header found in request header");
        else
            return false;
    }

    _onlySentHeaderFirst = (_bufferPosition == _headerBytesParsed);

    return true;
}

Method Request::GetMethod() const {
    auto method = ::http_method_str(static_cast<::http_method>(GetParser(_httpParser)->method));

    // indices must correspond to Method enum
    auto methods = {
        "OPTIONS",
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "TRACE",
        "CONNECT"
    };

    int i = 0;

    for (auto& m : methods) {
        if (!::strcmp(method, m))
            return static_cast<Method>(i);

        ++i;
    }

    throw std::runtime_error("Unknown HTTP method");
}

std::string_view Request::GetUri() const {
    return _uri;
}

Version Request::GetVersion() const {
    auto parser = GetParser(_httpParser);

    if (parser->http_major != 1)
        throw std::runtime_error("Unsupported HTTP method");

    if (parser->http_minor == 0)
        return Version::Http10;
    else if (parser->http_minor == 1)
        return Version::Http11;
    else
        throw std::runtime_error("Unsupported HTTP method");
}

std::vector<std::string_view> Request::GetHeaderNames() const {
    std::vector<std::string_view> result;

    result.reserve(_headers.size());

    for (auto& h : _headers)
        result.push_back(h.first);

    return result;
}

bool Request::HasHeader(const std::string_view& name) const {
    return GetHeader(name, nullptr);
}

bool Request::GetHeader(const std::string_view& name, std::string* value) const {
    for (auto& [headerName, headerValue] : _headers) {
        if (!::strncasecmp(headerName.data(), name.data(), name.size())) {
            if (value)
                *value = std::string(headerValue.data(), headerValue.size());

            return true;
        }
    }

    return false;
}

std::string_view Request::GetHeader(const std::string_view& name) const {
    for (auto& [headerName, headerValue] : _headers)
        if (!::strncasecmp(headerName.data(), name.data(), name.size()))
                return headerValue;

    throw std::runtime_error("Specified header does not exist");
}

bool Request::HasContent() const {
    return GetContentLength() != 0;
}

bool Request::IsContentAvailable() const {
    return GetContentLength() == _content.size();
}

std::size_t Request::GetContentLength() const {
    return GetParser(_httpParser)->content_length;
}

bool Request::KeepAlive() const {
    return ::http_should_keep_alive(GetParser(_httpParser));
}

bool Request::ConsumeContent(std::size_t maxBytes, std::size_t& totalBytesRead) {
    totalBytesRead = 0;

    /*
     * The initial request buffer is quite large.
     * There's a good chance it contains lots of data.
     * Therefore, we start by extracting all of the data
     * from the initial buffer, and only then do we proceed
     * to reading further input from the stream.
     *
     * The exception to this rule is if the client
     * only sent the header first and waited for body
     * retrievals to be done in subsequent ones.
     */
    auto contentLength = GetContentLength();

    if (contentLength > 0x100000000)
        throw std::runtime_error("Request body too big; rejected!");

    if (_contentPosition == 0) {
        _content.resize(contentLength);

        if (!_onlySentHeaderFirst) {
            auto nonHeaderDataInInitialBuffer = _bufferPosition - _headerBytesParsed;
            auto contentInInitialBuffer = std::min(nonHeaderDataInInitialBuffer, contentLength);

            // not modifying maxBytes or totalBytesRead on purpose,
            // since this is data we already received during our
            // header consumption stage and was already counted.
            auto remaining = contentInInitialBuffer;

            std::memcpy(_content.data() + _contentPosition, _buffer.data() + _headerBytesParsed, remaining);

            _contentPosition += remaining;

            if (_contentPosition == contentLength)
                return true;
        }
    }

    auto readLength = std::min(maxBytes, contentLength - _contentPosition);

    auto bytesRead = _input->Read(_content.data() + _contentPosition, readLength);

    _contentPosition += bytesRead;
    totalBytesRead += bytesRead;

    return _contentPosition == contentLength;
}

const std::vector<char>& Request::GetContent() const {
    return _content;
}

} // namespace Chili

