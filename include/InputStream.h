#pragma once

#include <chrono>
#include <cstddef>

namespace Yam {
namespace Http {

class InputStream {
public:
    virtual std::size_t Read(void*, std::size_t) = 0;
    virtual std::size_t Read(void* buffer, std::size_t maxBytes, std::chrono::milliseconds timeout);
    virtual bool Read(void*, std::size_t, std::size_t& readBytes);

    virtual ~InputStream() = default;
};

inline std::size_t InputStream::Read(void* buffer, std::size_t maxBytes, std::chrono::milliseconds) {
    return Read(buffer, maxBytes);
}

inline bool InputStream::Read(void* buffer, std::size_t maxBytes, std::size_t& readBytes) {
    readBytes = Read(buffer, maxBytes);
    return true;
}

} // namespace Http
} // namespace Yam

