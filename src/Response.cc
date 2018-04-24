#include "Response.h"
#include "Log.h"
#include "TcpConnection.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fmt/format.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <time.h>

namespace Chili {

namespace {
#define UPTR_PAIR(a, b) \
    {{ static_cast<std::uintptr_t>(a), reinterpret_cast<std::uintptr_t>(b) }}

std::string HttpVersion = "HTTP/1.1";
std::array<std::array<std::uintptr_t, 2>, 39> HttpStatusStrings = {{
    UPTR_PAIR(Status::Continue, "100 Continue"),
    UPTR_PAIR(Status::SwitchingProtocols, "101 Switching Protocols"),
    UPTR_PAIR(Status::Ok, "200 OK"),
    UPTR_PAIR(Status::Created, "201 Created"),
    UPTR_PAIR(Status::Accepted, "202 Accepted"),
    UPTR_PAIR(Status::NonAuthoritativeInformation, "203 Non-Authoritative Information"),
    UPTR_PAIR(Status::NoContent, "204 No Content"),
    UPTR_PAIR(Status::ResetContent, "205 Reset Content"),
    UPTR_PAIR(Status::PartialContent, "206 Partial Content"),
    UPTR_PAIR(Status::MultipleChoices, "300 Multiple Choices"),
    UPTR_PAIR(Status::MovedPermanently, "301 Moved Permanently"),
    UPTR_PAIR(Status::Found, "302 Found"),
    UPTR_PAIR(Status::SeeOther, "303 See Other"),
    UPTR_PAIR(Status::NotModified, "304 Not Modified"),
    UPTR_PAIR(Status::UseProxy, "305 Use Proxy"),
    // 306 is reserved
    UPTR_PAIR(Status::TemporaryRedirect, "307 Temporary Redirect"),
    UPTR_PAIR(Status::BadRequest, "400 Bad Request"),
    UPTR_PAIR(Status::Unauthorized, "401 Unauthorized"),
    // 402 is reserved
    UPTR_PAIR(Status::Forbidden, "403 Forbidden"),
    UPTR_PAIR(Status::NotFound, "404 Not Found"),
    UPTR_PAIR(Status::MethodNotAllowed, "405 Method Not Allowed"),
    UPTR_PAIR(Status::NotAcceptable, "406 Not Acceptable"),
    UPTR_PAIR(Status::ProxyAuthenticationRequired, "407 Proxy Authentication Required"),
    UPTR_PAIR(Status::RequestTimeout, "408 Request Timeout"),
    UPTR_PAIR(Status::Conflict, "409 Conflict"),
    UPTR_PAIR(Status::Gone, "410 Gone"),
    UPTR_PAIR(Status::LengthRequired, "411 Length Required"),
    UPTR_PAIR(Status::PreconditionFailed, "412 Precondition Failed"),
    UPTR_PAIR(Status::RequestEntityTooLarge, "413 Request Entity TooLarge"),
    UPTR_PAIR(Status::RequestUriTooLong, "414 Request-URI Too Long"),
    UPTR_PAIR(Status::UnsupportedMediaType, "415 Unsupported Media Type"),
    UPTR_PAIR(Status::RequestedRangeNotSatisfiable, "416 Requested Range Not Satisfiable"),
    UPTR_PAIR(Status::ExpectationFailed, "417 Expectation Failed"),
    UPTR_PAIR(Status::InternalServerError, "500 Internal Server Error"),
    UPTR_PAIR(Status::NotImplemented, "501 Not Implemented"),
    UPTR_PAIR(Status::BadGateway, "502 Bad Gateway"),
    UPTR_PAIR(Status::ServiceUnavailable, "503 Service Unavailable"),
    UPTR_PAIR(Status::GatewayTimeout, "504 Gateway Timeout"),
    UPTR_PAIR(Status::HttpVersionNotSupported, "505 Http Version Not Supported")
}};

const char* ToString(Status s) {
    auto&& pair = HttpStatusStrings.at(static_cast<std::uintptr_t>(s));
    return reinterpret_cast<const char*>(pair.at(1));
}

std::string CookieDate(const std::time_t& t) {
    char buffer[] = "Wdy, DD Mon YYYY HH:MM:SS GMT";
    struct tm tm;

    if (!::gmtime_r(&t, &tm))
        throw std::runtime_error("gmtime() failed");

    if(!std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %T GMT", &tm))
        throw std::runtime_error("Failed to format specified time");

    return buffer;
}

} // unnamed namespace

Response::Response(std::shared_ptr<OutputStream> stream,
                   std::weak_ptr<Signal<>> readyToWrite)
    : _stream(std::move(stream))
    , _readyToWrite(std::move(readyToWrite)) {}

void Response::Reset() {
    auto stream = std::move(_stream);
    auto readyToWrite = std::move(_readyToWrite);
    *this = Response(std::move(stream), std::move(readyToWrite));
}

void Response::SetStatus(Status status) {
    if (!_prepared)
        Prepare(status);
    _response->_status = status;
}

bool Response::IsPrepared() const {
    return _prepared;
}

void Response::UseCached(std::shared_ptr<CachedResponse> cr) {
    _response = std::move(cr);
    _prepared = true;
}

std::shared_ptr<CachedResponse> Response::Cache() {
    if (GetState()._stream)
        throw std::logic_error("Cannot cache response with streaming content");

    if (!_prepared)
        throw std::logic_error("Response attempted to be cached before being fully prepared");

    return _response;
}

void Response::Prepare(Status status) {
    fmt::MemoryWriter w;

    w.write("{} {}\r\n", HttpVersion, ToString(status));

    for (auto& [name, value] : _headers)
        w.write("{}: {}\r\n", name, value);

    if (GetState()._transferMode == TransferMode::Normal) {
        std::size_t contentLength;

        if (GetState()._strBody)
            contentLength = GetState()._strBody->size();
        else if (GetState()._body)
            contentLength = GetState()._body->size();
        else
            contentLength = 0;

        w.write("Content-Length: {}\r\n", contentLength);
    }

    w.write("\r\n");

    GetState()._header = w.str();

    GetState()._status = status;

    _prepared = true;
}

Response::FlushStatus Response::Flush(std::size_t maxBytes, std::size_t& totalBytesWritten) {
   auto& response = GetState();

   if (response._transferMode == TransferMode::Normal) {
       if (response._strBody)
           return FlushBody(*response._strBody, maxBytes, totalBytesWritten);
       else if (response._body)
           return FlushBody(*response._body, maxBytes, totalBytesWritten);
       else
           return FlushWithHeader(std::string(), maxBytes, totalBytesWritten);
   } else if (response._transferMode == TransferMode::Chunked) {
       if (response._stream)
           return FlushStream(maxBytes, totalBytesWritten);
       else
           throw std::logic_error("No stream provided");
   } else {
        throw std::logic_error("Unsupported transfer mode");
   }
}

template <class T>
Response::FlushStatus Response::FlushWithHeader(const T& data, std::size_t& maxBytes, std::size_t& totalBytesWritten) {
    auto& response = GetState();
    auto& header = response._header;

    if (_writePosition >= header.size())
        Log::Fatal("Invalid call to Response::FlushWithHeader()");

    auto vec = std::vector<std::pair<const void*, std::size_t>>();

    auto headerBytesRemaining = header.size() - _writePosition;

    if (maxBytes <= headerBytesRemaining || data.empty()) {
        auto quota = std::min(maxBytes, headerBytesRemaining);
        vec.push_back(std::make_pair(header.c_str() + _writePosition, quota));
    } else {
        auto dataQuota = std::min(data.size(), maxBytes - headerBytesRemaining);

        vec.push_back(std::make_pair(header.data() + _writePosition, headerBytesRemaining));
        vec.push_back(std::make_pair(data.data(), dataQuota));
    }

    auto bytesWritten = _stream->WriteVector(vec);

    totalBytesWritten += bytesWritten;
    _writePosition += bytesWritten;
    maxBytes -= bytesWritten;

    auto headerSent = _writePosition >= header.size();

    if (headerSent) {
        return FlushStatus::Done;
    } else {
        if (maxBytes > 0)
            return FlushStatus::IncompleteWrite;
        else
            return FlushStatus::ReachedQuota;
    }
}

template <class T>
Response::FlushStatus Response::FlushBody(const T& body, std::size_t& maxBytes, std::size_t& totalBytesWritten) {
    auto& response = GetState();
    auto& header = response._header;

    if (_writePosition < header.size()) {
        auto result = FlushWithHeader(body, maxBytes, totalBytesWritten);

        if (result != FlushStatus::Done)
            return result;

        if (maxBytes == 0)
            return FlushStatus::ReachedQuota;
    }

    if (_writePosition < header.size())
        Log::Fatal("Invalid write state in Response::FlushBody()");

    auto bodyBytesConsumed = _writePosition - header.size();
    auto quota = std::min(maxBytes, body.size() - bodyBytesConsumed);
    auto bytesWritten = _stream->Write(body.data() + bodyBytesConsumed, quota);

    totalBytesWritten += bytesWritten;
    _writePosition += bytesWritten;

    auto sentAll = (_writePosition - header.size()) == body.size();

    if (sentAll) {
        return FlushStatus::Done;
    } else {
        if (maxBytes > 0)
            return FlushStatus::IncompleteWrite;
        else
            return FlushStatus::ReachedQuota;
    }
}

Response::FlushStatus Response::FlushStream(std::size_t& maxBytes, std::size_t& totalBytesWritten) {
    auto& response = GetState();
    auto& header = response._header;

    if (_writePosition < header.size()) {
        auto result = FlushWithHeader(std::string(), maxBytes, totalBytesWritten);

        if (result != FlushStatus::Done)
            return result;

        if (maxBytes == 0)
            return FlushStatus::ReachedQuota;
    }

    if (_needNewChunk) {
        auto result = ReadNextChunk();

        if (result == ReadResult::Buffering)
            return FlushStatus::WaitingForContent;
    }

    auto bytesWritten = _stream->WriteVector(GetChunkVector(maxBytes));

    maxBytes -= bytesWritten;
    totalBytesWritten += bytesWritten;

    UpdateChunkWritePositions(bytesWritten);

    if (_chunkWritePosition < _chunkSize || _chunkTrailWritePosition < 2) {
        if (maxBytes > 0)
            return FlushStatus::IncompleteWrite;
        else
            return FlushStatus::ReachedQuota;
    } else {
        if (_isLastChunk) {
            return FlushStatus::Done;
        } else {
            _needNewChunk = true;
            return FlushStatus::Repeat;
        }
    }
}

void Response::UpdateChunkWritePositions(std::size_t bytesWritten) {
    if (_chunkHeaderWritePosition < _chunkHeader.size()) {
        auto chunkHeaderRemaining = _chunkHeader.size() - _chunkHeaderWritePosition;
        auto chunkHeaderWritten = std::min(bytesWritten, chunkHeaderRemaining);

        _chunkHeaderWritePosition += chunkHeaderWritten;
        bytesWritten -= chunkHeaderWritten;

        if (bytesWritten == 0 || _chunkHeaderWritePosition < _chunkHeader.size())
            return;
    }

    if (_chunkWritePosition < _chunkSize) {
        auto chunkBodyRemaining = _chunkSize - _chunkWritePosition;
        auto chunkBodyWritten = std::min(bytesWritten, chunkBodyRemaining);

        _chunkWritePosition += chunkBodyWritten;
        bytesWritten -= chunkBodyWritten;

        if (bytesWritten == 0 || _chunkWritePosition < _chunkSize)
            return;
    }

    if (_chunkTrailWritePosition < 2) {
        auto chunkTrailRemaining = 2 - _chunkTrailWritePosition;
        auto chunkTrailWritten = std::min(bytesWritten, chunkTrailRemaining);

        _chunkTrailWritePosition += chunkTrailWritten;
    }
}

Response::ReadResult Response::ReadNextChunk() {
    auto& response = GetState();
    auto& input = response._stream;
    auto& buffer = response._body;

    if (input->EndOfStream()) {
        _chunkHeader = "0\r\n";
        _chunkSize = 0;
        _chunkWritePosition = 0;
        _chunkHeaderWritePosition = 0;
        _chunkTrailWritePosition = 0;
        _needNewChunk = false;
        _isLastChunk = true;
        return ReadResult::DataAvailable;
    }

    if (auto bufferedInput = std::dynamic_pointer_cast<BufferedInputStream>(input)) {
        if (bufferedInput->GetBufferedInputSize() == 0) {
            bufferedInput->BufferInputAsync();
            return ReadResult::Buffering;
        }
    }

    _chunkSize = input->Read(buffer->data(), buffer->size());
    _chunkHeader = fmt::format("{:X}\r\n", _chunkSize);

    _chunkWritePosition = 0;
    _chunkHeaderWritePosition = 0;
    _chunkTrailWritePosition = 0;
    _needNewChunk = false;

    return ReadResult::DataAvailable;
}

std::vector<std::pair<const void*, std::size_t>> Response::GetChunkVector(std::size_t maxBytes) {
    auto vec = std::vector<std::pair<const void*, std::size_t>>();

    auto& chunk = GetState()._body;
    auto chunkHeaderRemaining = _chunkHeader.size() - _chunkHeaderWritePosition;
    auto chunkBodyRemaining = _chunkSize - _chunkWritePosition;

    if (chunkHeaderRemaining > 0) {
        auto headerQuota = std::min(maxBytes, chunkHeaderRemaining);
        vec.push_back(std::make_pair(_chunkHeader.data() + _chunkHeaderWritePosition, headerQuota));

        maxBytes -= headerQuota;

        if (_chunkSize > 0 && maxBytes > 0) {
            auto dataQuota = std::min(maxBytes, _chunkSize);
            vec.push_back(std::make_pair(chunk->data(), dataQuota));

            maxBytes -= dataQuota;

            if (maxBytes > 0) {
                auto quota = std::min(maxBytes, 2lu);
                vec.push_back(std::make_pair("\r\n", quota));
            }
        }
    } else if (chunkBodyRemaining > 0) {
        auto quota = std::min(maxBytes, chunkBodyRemaining);
        vec.push_back(std::make_pair(chunk->data() + _chunkWritePosition, quota));

        maxBytes -= quota;

        if (maxBytes > 0) {
            auto quota = std::min(maxBytes, 2lu);
            vec.push_back(std::make_pair("\r\n", quota));
        }
    } else {
        auto chunkTrailRemaining = 2lu - _chunkTrailWritePosition;
        auto quota = std::min(maxBytes, chunkTrailRemaining);
        auto trail = "\r\n";
        vec.push_back(std::make_pair(trail + _chunkTrailWritePosition, quota));
    }

    return vec;
}

bool Response::GetKeepAlive() const {
    return GetState()._keepAlive;
}

void Response::CloseConnection() {
    AppendHeader("Connection", "close");
    GetState()._keepAlive = false;
}

void Response::KeepConnectionAlive() {
    AppendHeader("Connection", "keep-alive");
    GetState()._keepAlive = true;
}

void Response::AppendHeader(std::string name, std::string value) {
    _headers.emplace_back(std::move(name), std::move(value));
}

void Response::SetCookie(std::string name, std::string value) {
    AppendHeader("Set-Cookie", fmt::format("{}={}", std::move(name), std::move(value)));
}

void Response::SetCookie(std::string name, std::string value, const CookieOptions& opts) {
    fmt::MemoryWriter w;
    std::string stringOpt;
    std::chrono::seconds durationOpt;
    std::time_t expirationOpt;

    if (opts.GetDomain(&stringOpt))
        w << "; Domain=" << stringOpt;

    if (opts.GetPath(&stringOpt))
        w << "; Path=" << stringOpt;

    if (opts.GetMaxAge(&durationOpt))
        w << "; Max-Age=" << durationOpt.count();

    if (opts.GetExpiration(&expirationOpt))
        w << "; Expires=" << CookieDate(expirationOpt);

    if (opts.IsHttpOnly())
        w << "; HttpOnly";

    if (opts.IsSecure())
        w << "; Secure";

    AppendHeader("Set-Cookie", fmt::format("{}={}{}", std::move(name), std::move(value), w.str()));
}

void Response::SetContent(std::string body) {
    auto& r = GetState();
    r._transferMode = TransferMode::Normal;
    r._strBody = std::make_shared<std::string>(std::move(body));
    r._body.reset();
    r._stream.reset();
}

void Response::SetContent(std::shared_ptr<std::vector<char>> body) {
    auto& r = GetState();
    r._transferMode = TransferMode::Normal;
    r._body = std::move(body);
    r._strBody.reset();
    r._stream.reset();
}

void Response::SetContent(std::shared_ptr<InputStream> stream) {
    if (auto s = std::dynamic_pointer_cast<BufferedInputStream>(stream)) {
        s->OnInputBuffered += [this] {
            if (auto signal = _readyToWrite.lock())
                signal->Raise();
        };
    }

    auto& r = GetState();
    r._transferMode = TransferMode::Chunked;
    r._stream = std::move(stream);
    r._body = std::make_shared<std::vector<char>>(GetBufferSize()); // use as buffer
    AppendHeader("Transfer-Encoding", "chunked");
    r._strBody.reset();
}

std::size_t Response::GetBufferSize() const {
    if (GetState()._transferMode == TransferMode::Chunked)
        return 0x1000;
    else
        return std::numeric_limits<std::size_t>::max();
}

Status Response::GetStatus() const {
    return GetState()._status;
}

CachedResponse& Response::GetState() const {
    if (!_response)
        _response = std::make_shared<CachedResponse>();
    return *_response;
}

} // namespace Chili

