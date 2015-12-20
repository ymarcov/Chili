#include "SocketStream.h"
#include "SystemError.h"

#include <fcntl.h>
#include <sys/socket.h>

namespace Yam {
namespace Http {

#define ENSURE(expr) \
    while (-1 == (expr)) \
        if (errno != EINTR) \
            throw SystemError{};

Socket& Socket::IncrementUseCount(Socket& s) {
    if (s._nativeHandle != InvalidHandle)
        ++*s._useCount;
    return s;
}

Socket::Socket() :
    FileStream{} {}

Socket::Socket(Socket::NativeHandle nh) :
    FileStream{nh} {
    if (_nativeHandle != InvalidHandle)
        _useCount = std::make_shared<std::atomic_int>(1);
}

Socket::Socket(const Socket& rhs) :
    FileStream{rhs},
    _useCount{rhs._useCount} {
    if (_nativeHandle != InvalidHandle)
        ++*_useCount;
}

Socket::Socket(Socket&& rhs) :
    FileStream{std::move(IncrementUseCount(rhs))} {
    _useCount = std::move(rhs._useCount);
    --*_useCount;
}

Socket& Socket::operator=(const Socket& rhs) {
    FileStream::operator=(rhs);
    _useCount = rhs._useCount;

    if (_nativeHandle != InvalidHandle)
        ++*_useCount;

    return *this;
}

Socket& Socket::operator=(Socket&& rhs) {
    if (rhs._nativeHandle == InvalidHandle) {
        FileStream::operator=(std::move(rhs));
    } else {
        FileStream::operator=(std::move(IncrementUseCount(rhs)));
        _useCount = std::move(rhs._useCount);
        --*_useCount;
    }
    return *this;
}

std::size_t Socket::Write(const void* buffer, std::size_t maxBytes) {
    ::ssize_t result;
    ENSURE(result = ::send(_nativeHandle, buffer, maxBytes, MSG_NOSIGNAL));
    return result;
}

std::size_t Socket::WriteTo(FileStream& fs, std::size_t maxBytes) {
    ::ssize_t result = ::splice(_nativeHandle,
                                nullptr,
                                fs.GetNativeHandle(),
                                nullptr,
                                maxBytes,
                                0);
    if (result == -1)
        throw SystemError{};

    return result;
}

void Socket::Close() {
    if ((_nativeHandle != InvalidHandle) && !--*_useCount)
        Shutdown();
    FileStream::Close();
}

void Socket::Shutdown() {
    int result;
    ENSURE(result = ::shutdown(_nativeHandle, SHUT_RDWR));
}

} // namespace Http
} // namespace Yam

