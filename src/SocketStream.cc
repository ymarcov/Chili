#include "SocketStream.h"
#include "Log.h"
#include "SystemError.h"

#include <fcntl.h>
#include <sys/socket.h>

namespace Chili {

SocketStream& SocketStream::IncrementUseCount(SocketStream& s) {
    if (s._nativeHandle != InvalidHandle)
        ++*s._useCount;
    return s;
}

SocketStream::SocketStream() :
    FileStream{} {}

SocketStream::SocketStream(SocketStream::NativeHandle nh) :
    FileStream{nh} {
    if (_nativeHandle != InvalidHandle)
        _useCount = std::make_shared<std::atomic_int>(1);
}

SocketStream::SocketStream(const SocketStream& rhs) :
    FileStream{rhs},
    _useCount{rhs._useCount} {
    if (_nativeHandle != InvalidHandle)
        ++*_useCount;
}

SocketStream::SocketStream(SocketStream&& rhs) :
    FileStream{std::move(IncrementUseCount(rhs))} {
    _useCount = std::move(rhs._useCount);
    --*_useCount;
}

SocketStream& SocketStream::operator=(const SocketStream& rhs) {
    FileStream::operator=(rhs);
    _useCount = rhs._useCount;

    if (_nativeHandle != InvalidHandle)
        ++*_useCount;

    return *this;
}

SocketStream& SocketStream::operator=(SocketStream&& rhs) {
    if (rhs._nativeHandle == InvalidHandle) {
        FileStream::operator=(std::move(rhs));
    } else {
        FileStream::operator=(std::move(IncrementUseCount(rhs)));
        _useCount = std::move(rhs._useCount);
        --*_useCount;
    }
    return *this;
}

std::size_t SocketStream::Write(const void* buffer, std::size_t maxBytes) {
    auto bytesWritten = ::send(_nativeHandle, buffer, maxBytes, MSG_NOSIGNAL);

    if (bytesWritten == -1)
        throw SystemError{};

    return bytesWritten;
}

std::size_t SocketStream::WriteTo(FileStream& fs, std::size_t maxBytes) {
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

void SocketStream::Close() {
    if ((_nativeHandle != InvalidHandle) && !--*_useCount)
        Shutdown();
    FileStream::Close();
}

void SocketStream::Shutdown() {
    if (-1 == ::shutdown(_nativeHandle, SHUT_RDWR))
        Log::Default()->Debug("Failed to shutdown socket fd {}", _nativeHandle);
}

} // namespace Chili

