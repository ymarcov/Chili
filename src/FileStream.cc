#include "FileStream.h"
#include "SystemError.h"

#include <sys/sendfile.h>
#include <unistd.h>

namespace Yam {
namespace Http {

#define ENSURE(expr) \
    while (-1 == (expr)) \
        if (errno != EINTR) \
            throw SystemError{};

const FileStream::NativeHandle FileStream::InvalidHandle = -1;

FileStream::FileStream() :
    _nativeHandle{InvalidHandle} {}

FileStream::FileStream(FileStream::NativeHandle nh) :
    _nativeHandle{nh} {}

FileStream::FileStream(const FileStream& rhs) {
    int result;
    ENSURE(result = ::dup(rhs._nativeHandle));
    _nativeHandle = result;
}

FileStream::FileStream(FileStream&& rhs) {
    _nativeHandle = rhs._nativeHandle.exchange(InvalidHandle);
}

FileStream::~FileStream() {
    try {
        Close();
    } catch (const std::exception&) {
        // log?
    }
}

FileStream& FileStream::operator=(const FileStream& rhs) {
    int result = InvalidHandle;

    if (rhs._nativeHandle != InvalidHandle)
        ENSURE(result = ::dup(rhs._nativeHandle));

    Close();
    _nativeHandle = result;

    return *this;
}

FileStream& FileStream::operator=(FileStream&& rhs) {
    auto result = rhs._nativeHandle.exchange(InvalidHandle);

    Close();
    _nativeHandle = result;

    return *this;
}

FileStream::NativeHandle FileStream::GetNativeHandle() const {
    return _nativeHandle;
}

std::size_t FileStream::Read(void* buffer, std::size_t maxBytes) {
    ::ssize_t result;
    ENSURE(result = ::read(_nativeHandle, buffer, maxBytes));
    return result;
}

std::size_t FileStream::Write(const void* buffer, std::size_t maxBytes) {
    ::ssize_t result;
    ENSURE(result = ::write(_nativeHandle, buffer, maxBytes));
    return result;
}

std::size_t FileStream::WriteTo(FileStream& fs, std::size_t maxBytes) {
    ::ssize_t result = ::sendfile(fs._nativeHandle,
                                  _nativeHandle,
                                  nullptr,
                                  maxBytes);

    if (result == -1)
        throw SystemError{};

    return result;
}

void FileStream::Close() {
    if (_nativeHandle != InvalidHandle) {
        int result;
        ENSURE(result = ::close(_nativeHandle));
    }
}

void SeekableFileStream::Seek(std::size_t offset) {
    if (-1 == ::lseek(_nativeHandle, 0, SEEK_SET))
        throw SystemError{};
}

} // namespace Http
} // namespace Yam

