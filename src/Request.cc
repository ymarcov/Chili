#include "Request.h"
#include "SystemError.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <strings.h>

namespace Yam {
namespace Http {

namespace {

bool BufferContainsEndOfHeaderMarker(const char* buffer, std::size_t bufferSize) {
    const std::uint32_t eohMarker = 0x0A0D0A0D;

    if (bufferSize < sizeof(eohMarker))
        return false;

    for (auto i = 0U; i < (bufferSize - 3); i++)
        if (*reinterpret_cast<const std::uint32_t*>(buffer + i) == eohMarker)
            return true;

    return false;
}

} // unnamed namespace

Request::Request(std::shared_ptr<void> emptyBuffer, std::shared_ptr<InputStream> input) :
    _buffer{std::move(emptyBuffer)},
    _input{std::move(input)} {
}

std::pair<bool, std::size_t> Request::ConsumeHeader(std::size_t maxBytes) {
    auto buffer = static_cast<char*>(_buffer.get());

    if (_bufferPosition == 0)
        std::memset(buffer, 0, sizeof(Buffer));

    auto quota = std::min(maxBytes, sizeof(Buffer) - _bufferPosition);
    auto bytesRead = _input->Read(buffer + _bufferPosition, quota);

    if (!bytesRead)
        return std::make_pair(false, std::size_t(0));

    _bufferPosition += bytesRead;

    // When we have the end-of-header marker, we can continue to the parsing stage

    if (!BufferContainsEndOfHeaderMarker(buffer, _bufferPosition)) {
        if (_bufferPosition == sizeof(Buffer))
            throw std::runtime_error("No end-of-header found in request header");

        return std::make_pair(false, bytesRead);
    }

    _parser = Parser::Parse(static_cast<char*>(_buffer.get()), _bufferPosition);
    _onlySentHeaderFirst = (_bufferPosition == _parser.GetHeaderLength());

    return std::make_pair(true, bytesRead);
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

std::string Request::GetUri() const {
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

    throw std::runtime_error("Unknown HTTP method");
}

std::vector<std::string> Request::GetFieldNames() const {
    std::vector<std::string> result;
    for (auto& f : _parser.GetFieldNames())
        result.emplace_back(f.Data, f.Size);
    return result;
}

bool Request::GetField(const std::string& name, std::string* value) const {
    Parser::Field f;

    if (_parser.GetField(name, &f)) {
        if (value)
            *value = {f.Data, f.Size};
        return true;
    }

    return false;
}

std::string Request::GetField(const std::string& name) const {
    auto f = _parser.GetField(name);
    return {f.Data, f.Size};
}

std::string Request::GetCookie(const std::string& name) const {
    auto f = _parser.GetCookie(name);
    return {f.Data, f.Size};
}

std::vector<std::string> Request::GetCookieNames() const {
    std::vector<std::string> result;
    for (auto& f : _parser.GetCookieNames())
        result.emplace_back(f.Data, f.Size);
    return result;
}

std::size_t Request::GetContentLength() const {
    try {
        return stoi(GetField("Content-Length"));
    } catch (const Parser::Error&) {
        return 0;
    }
}

bool Request::KeepAlive() const {
    Parser::Field f;

    if (_parser.GetField("connection", &f))
        return !::strncasecmp(f.Data, "keep-alive", f.Size);

    return false;
}

std::pair<bool, std::size_t> Request::ConsumeBody(void* buffer, std::size_t bufferSize) {
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

    if (!_onlySentHeaderFirst) {
        auto nonHeaderDataInInitialBuffer = sizeof(Buffer) - _parser.GetHeaderLength();
        auto contentInInitialBuffer = std::min(nonHeaderDataInInitialBuffer, contentLength);

        if (_contentBytesReadFromInitialBuffer < contentInInitialBuffer) {
            auto remaining = contentInInitialBuffer - _contentBytesReadFromInitialBuffer;
            auto bytesRead = std::min(remaining, bufferSize);

            std::memcpy(buffer, _parser.GetBody() + _contentBytesReadFromInitialBuffer, bytesRead);
            _contentBytesReadFromInitialBuffer += bytesRead;

            reinterpret_cast<char*&>(buffer) += bytesRead;
            bufferSize -= bytesRead;
            contentLength -= _contentBytesReadFromInitialBuffer;

            if (!contentLength)
                return std::make_pair(true, bytesRead);

            if (!bufferSize)
                return std::make_pair(false, bytesRead);
        }
    }

    auto readLength = std::min(bufferSize, contentLength);

    if (readLength == 0)
        return std::make_pair(true, std::size_t(0));

    auto bytesRead = _input->Read(buffer, readLength);

    return std::make_pair(bytesRead == contentLength, bytesRead);
}

} // namespace Http
} // namespace Yam

