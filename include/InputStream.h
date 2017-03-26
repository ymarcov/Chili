#pragma once

#include <chrono>
#include <cstddef>

namespace Yam {
namespace Http {

/**
 * A stream of bytes.
 *
 * This can be used for streaming content from the server.
 */
class InputStream {
public:
    /**
     * Reads @p n bytes into @p buffer.
     *
     * @param buffer The buffer to read into.
     * @param n      Maximum number of bytes to read.
     * @return       The number of bytes actually read into the buffer.
     */
    virtual std::size_t Read(void* buffer, std::size_t n) = 0;

    /**
     * Returns true if the end of the stream has been reached.
     */
    virtual bool EndOfStream() const = 0;

    /**
     * @internal
     */
    virtual std::size_t Read(void* buffer, std::size_t maxBytes, std::chrono::milliseconds timeout);

    virtual ~InputStream() = default;
};

inline std::size_t InputStream::Read(void* buffer, std::size_t maxBytes, std::chrono::milliseconds) {
    return Read(buffer, maxBytes);
}

} // namespace Http
} // namespace Yam

