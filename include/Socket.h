#pragma once

#include "InputStream.h"
#include "OutputStream.h"

namespace Yam {
namespace Http {

class Socket : public InputStream, public OutputStream {
public:
    using NativeHandle = int;

    Socket() = default;
    Socket(NativeHandle fd);
    Socket(const Socket&) = delete;
    Socket(Socket&&);

    virtual ~Socket();

    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&&);

    NativeHandle GetNativeHandle() const;
    std::size_t Read(void* buffer, std::size_t bufferSize) override;
    std::size_t Write(const void* buffer, std::size_t bufferSize) override;

private:
    void Close();

    NativeHandle _fd = -1;
    bool _null = true;
};

inline Socket::NativeHandle Socket::GetNativeHandle() const {
    return _fd;
}

} // namespace Http
} // namespace Yam

