#include "Request.h"
#include "SystemError.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <strings.h>

namespace Yam {
namespace Http {

constexpr std::size_t BufferSize = 0x2000;

namespace {

bool BufferContainsEndOfHeaderMarker(const char* buffer, std::size_t bufferSize) {
    const std::uint32_t eohMarker = 0x0A0D0A0D;

    if (bufferSize < sizeof(eohMarker))
        return false;

    for (auto i = 0U; i <= (bufferSize - sizeof(std::uint32_t)); i++)
        if (*reinterpret_cast<const std::uint32_t*>(buffer + i) == eohMarker)
            return true;

    return false;
}

} // unnamed namespace

Request::Request(std::shared_ptr<InputStream> input) :
    _buffer(BufferSize),
    _input{std::move(input)} {}

bool Request::ConsumeHeader(std::size_t maxBytes, std::size_t& bytesRead) {
    if (_bufferPosition == 0)
        std::memset(_buffer.data(), 0, BufferSize);

    auto quota = std::min(maxBytes, BufferSize - _bufferPosition);
    bytesRead = _input->Read(_buffer.data() + _bufferPosition, quota);

    if (!bytesRead)
        return false;

    _bufferPosition += bytesRead;

    // When we have the end-of-header marker, we can continue to the parsing stage

    if (!BufferContainsEndOfHeaderMarker(_buffer.data(), _bufferPosition)) {
        if (_bufferPosition == BufferSize)
            throw std::runtime_error("No end-of-header found in request header");

        return false;
    }

    _parser = Parser::Parse(static_cast<char*>(_buffer.data()), _bufferPosition);
    _onlySentHeaderFirst = (_bufferPosition == _parser.GetHeaderLength());

    return true;
}

Method Request::GetMethod() const {
    auto field = _parser.GetMethod();

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
        if (!::strncasecmp(field.Data, m, field.Size))
            return static_cast<Method>(i);
        ++i;
    }

    throw std::runtime_error("Unknown HTTP method");
}

std::string_view Request::GetUri() const {
    auto f = _parser.GetUri();
    return {f.Data, f.Size};
}

Version Request::GetVersion() const {
    auto field = _parser.GetVersion();

    // indices must correspond to Version
    auto versions = {
        "HTTP/1.0",
        "HTTP/1.1"
    };

    int i = 0;
    for (auto& v : versions) {
        if (!::strncasecmp(field.Data, v, field.Size))
            return static_cast<Version>(i);
        ++i;
    }

    throw std::runtime_error("Unsupported HTTP method");
}

std::vector<std::string_view> Request::GetFieldNames() const {
    std::vector<std::string_view> result;
    for (auto& f : _parser.GetFieldNames())
        result.emplace_back(f.Data, f.Size);
    return result;
}

bool Request::HasField(const std::string_view& name) const {
    return GetField(name, nullptr);
}

bool Request::GetField(const std::string_view& name, std::string* value) const {
    Parser::Field f;

    if (_parser.GetField(name, &f)) {
        if (value)
            *value = {f.Data, f.Size};
        return true;
    }

    return false;
}

std::string_view Request::GetField(const std::string_view& name) const {
    auto f = _parser.GetField(name);
    return {f.Data, f.Size};
}

std::string_view Request::GetCookie(const std::string_view& name) const {
    auto f = _parser.GetCookie(name);
    return {f.Data, f.Size};
}

std::vector<std::string_view> Request::GetCookieNames() const {
    std::vector<std::string_view> result;
    for (auto& f : _parser.GetCookieNames())
        result.emplace_back(f.Data, f.Size);
    return result;
}

bool Request::HasContent() const {
    Parser::Field f;

    if (!_parser.GetField("Content-Length", &f))
        return false;

    return !!std::strncmp(f.Data, "0", f.Size);
}

bool Request::IsContentAvailable() const {
    return GetContentLength() == _content.size();
}

std::size_t Request::GetContentLength() const {
    try {
        char* endptr;
        return strtoul(GetField("Content-Length").data(), &endptr, 10);
    } catch (const Parser::Error&) {
        return 0;
    }
}

bool Request::KeepAlive() const {
    Parser::Field f;

    if (_parser.GetField("connection", &f)) {
        if (!::strncasecmp(f.Data, "close", f.Size))
            return false;

        if (!::strncasecmp(f.Data, "keep-alive", f.Size))
            return true;
    }

    f = _parser.GetVersion();
    return !::strncasecmp(f.Data, "HTTP/1.1", f.Size);
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
    // FIXME: what if too big? potential hole
    auto contentLength = GetContentLength();

    if (_contentPosition == 0) {
        _content.resize(contentLength);

        if (!_onlySentHeaderFirst) {
            auto nonHeaderDataInInitialBuffer = _bufferPosition - _parser.GetHeaderLength();
            auto contentInInitialBuffer = std::min(nonHeaderDataInInitialBuffer, contentLength);

            // not modifying maxBytes or totalBytesRead on purpose,
            // since this is data we already received during out
            // header consumption stage and was already counted.
            auto remaining = contentInInitialBuffer;

            std::memcpy(_content.data() + _contentPosition, _parser.GetBody(), remaining);

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

} // namespace Http
} // namespace Yam

