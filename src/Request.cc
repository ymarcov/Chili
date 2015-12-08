#include "Request.h"
#include "SystemError.h"

#include <algorithm>
#include <cstring>
#include <strings.h>

namespace Yam {
namespace Http {

Request::Request(MemorySlot<Buffer> emptyBuffer, std::shared_ptr<InputStream> input) :
    _buffer{std::move(emptyBuffer)},
    _input{std::move(input)} {
    auto bytesRead = _input->Read(_buffer.get(), sizeof(Buffer));
    _parser = Parser::Parse(_buffer.get(), bytesRead);
    _onlySentHeaderFirst = (bytesRead == _parser.GetHeaderLength());
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

std::size_t Request::ReadNextBodyChunk(void* buffer, std::size_t bufferSize) {
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

            if (!bufferSize || !contentLength)
                return bytesRead;
            else
                ; // fallthrough
        }
    }

    return _input->Read(buffer, std::min(bufferSize, contentLength));
}

} // namespace Http
} // namespace Yam

