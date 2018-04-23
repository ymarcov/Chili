#pragma once

#include "BufferedInputStream.h"
#include "InputStream.h"
#include "OutputStream.h"
#include "Protocol.h"
#include "Signal.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Chili {

class CachedResponse {
    TransferMode _transferMode;
    Status _status;
    bool _keepAlive = true;
    std::string _header;
    std::shared_ptr<InputStream> _stream;
    std::shared_ptr<std::string> _strBody;
    std::shared_ptr<std::vector<char>> _body;

    friend class Response;
};

/**
 * An HTTP response.
 */
class Response {
public:
    /**
     * @internal
     */
    enum class FlushStatus {
        ReachedQuota,
        IncompleteWrite,
        WaitingForContent,
        Repeat,
        Done
    };

    Response() = default;
    Response(std::shared_ptr<OutputStream>,
             std::weak_ptr<Signal<>> readyToWrite);

    /**
     * Resets the state of the response.
     * This might be useful if you're going to change
     * whatever properties you set earlier, for example
     * if now you're going to send a different kind of
     * response, like an error.
     */
    void Reset();

    /**
     * Creates a cached response which can later
     * be sent through to improve efficiency.
     *
     * NOTE: A response that has a stream as its
     * content cannot be cached, and attempting
     * to cache it will throw an error.
     */
    std::shared_ptr<CachedResponse> Cache();

    /**
     * Instructs the client to keep the connection
     * associated with this request alive after
     * handling the request.
     */
    void KeepConnectionAlive();

    /**
     * Instructs the client to close the connection.
     */
    void CloseConnection();

    /**
     * Sets a response header.
     *
     * @param name  The HTTP response header name
     * @param value The header's value
     */
    void AppendHeader(std::string name, std::string value);

    /**
     * Sets a cookie.
     *
     * @param name  The name of the cookie
     * @param value The value of the cookie
     */
    void SetCookie(std::string name, std::string value);

    /**
     * Sets a cookie with extra cookie options.
     *
     * @param name  The name of the cookie
     * @param value The value of the cookie
     * @param opts  Extra options for the cookie
     */
    void SetCookie(std::string name, std::string value, const CookieOptions& opts);

    /**
     * Sets the response message content (e.g. HTML, or some file data).
     *
     * @param data The data to be sent
     */
    void SetContent(std::string data);

    /**
     * Sets the response message content (e.g. HTML, or some file data).
     *
     * @param data The data to be sent
     */
    void SetContent(std::shared_ptr<std::vector<char>> data);

    /**
     * Sets the response message content from a stream.
     * This will cause the response to be chunked.
     *
     * @param stream The stream containing the data to be sent
     */
    void SetContent(std::shared_ptr<InputStream> stream);

    /**
     * Sets the status of the response.
     */
    void SetStatus(Status);

    /**
     * Uses a previously cached response as this response.
     */
    void UseCached(std::shared_ptr<CachedResponse>);

    /**
     * @internal
     * Writes response data to the output stream.
     * Returns the way in which the operation ended,
     * and how many bytes were written (as an out variable).
     */
    FlushStatus Flush(std::size_t maxBytes, std::size_t& consumed);

    /**
     * @internal
     */
    bool GetKeepAlive() const;

    /**
     * @internal
     * Gets the status that was requested to be sent.
     */
    Status GetStatus() const;

    /**
     * @internal
     * Gets the chunk buffer size.
     */
    std::size_t GetBufferSize() const;

    /**
     * @internal
     */
    bool IsPrepared() const;

private:
    enum class ReadResult {
        Buffering,
        NewChunk,
        EndOfMessageChunk
    };

    void Prepare(Status);
    CachedResponse& GetState() const;

    template <class T>
    FlushStatus FlushWithHeader(const T& data, std::size_t& maxBytes, std::size_t& consumed);

    template <class T>
    FlushStatus FlushBody(const T& data, std::size_t& maxBytes, std::size_t& consumed);

    FlushStatus FlushStream(std::size_t& maxBytes, std::size_t& consumed);

    ReadResult ReadNextChunk();
    std::vector<std::pair<const void*, std::size_t>> GetChunkVector(std::size_t maxBytes);
    void UpdateChunkWritePositions(std::size_t bytesWritten);

    std::shared_ptr<OutputStream> _stream;
    std::weak_ptr<Signal<>> _readyToWrite;
    bool _prepared = false;
    std::vector<std::pair<std::string, std::string>> _headers;
    mutable std::shared_ptr<CachedResponse> _response;
    std::size_t _writePosition = 0;
    std::size_t _chunkWritePosition = 0;
    std::size_t _chunkSize;
    std::string _chunkHeader;
    std::size_t _chunkHeaderWritePosition = 0;
    std::size_t _chunkTrailWritePosition = 0;
    bool _needNewChunk = true;
    bool _isLastChunk = false;
};

} // namespace Chili

