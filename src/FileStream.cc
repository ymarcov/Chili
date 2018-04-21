#include "FileStream.h"
#include "Log.h"
#include "SystemError.h"
#include "Timeout.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Chili {

const FileStream::NativeHandle FileStream::InvalidHandle = -1;

namespace {

int FileModeToNative(FileMode mode) {
    int result = 0;

    if ((mode & (FileMode::Read | FileMode::Write)) == (FileMode::Read | FileMode::Write))
        result |= O_RDWR;
    else if (mode & FileMode::Read)
        result |= O_RDONLY;
    else if (mode & FileMode::Write)
        result |= O_WRONLY;

    if (mode & FileMode::Append)
        result |= O_APPEND;

    if (mode & FileMode::Create)
        result |= O_CREAT;

    if (mode & FileMode::Truncate)
        result |= O_TRUNC;

    return result;
}

} // unnamed namespace

std::unique_ptr<FileStream> FileStream::Open(const std::string& path, FileMode mode) {
    auto fd = ::open(path.c_str(), FileModeToNative(mode));

    if (fd == -1)
        throw SystemError{};

    struct ::stat statbuf;

    if (::fstat(fd, &statbuf) == -1)
        throw SystemError();

    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
        throw std::runtime_error("Specified path is a directory");

    return std::make_unique<FileStream>(fd);
}

FileStream::FileStream() :
    _nativeHandle{InvalidHandle} {}

FileStream::FileStream(FileStream::NativeHandle nh) :
    _nativeHandle{nh} {}

FileStream::FileStream(const FileStream& rhs) {
    _nativeHandle = ::dup(rhs._nativeHandle);
    _endOfStream = rhs._endOfStream;

    if (_nativeHandle == -1)
        throw SystemError{};
}

FileStream::FileStream(FileStream&& rhs) {
    _nativeHandle = rhs._nativeHandle.exchange(InvalidHandle);
    _endOfStream = rhs._endOfStream;
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

    if (rhs._nativeHandle != InvalidHandle) {
        result = ::dup(rhs._nativeHandle);

        if (result == -1)
            throw SystemError{};
    }

    Close();

    _nativeHandle = result;
    _endOfStream = rhs._endOfStream;

    return *this;
}

FileStream& FileStream::operator=(FileStream&& rhs) {
    auto result = rhs._nativeHandle.exchange(InvalidHandle);

    Close();

    _nativeHandle = result;
    _endOfStream = rhs._endOfStream;

    return *this;
}

FileStream::NativeHandle FileStream::GetNativeHandle() const {
    return _nativeHandle;
}

void FileStream::SetBlocking(bool blocking) {
    auto flags = ::fcntl(_nativeHandle, F_GETFL, 0);

    if (flags == -1)
        throw SystemError{};

    auto newFlags = (!blocking) ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);

    if (-1 == ::fcntl(_nativeHandle, F_SETFL, newFlags))
        throw SystemError{};
}

std::size_t FileStream::Read(void* buffer, std::size_t maxBytes) {
    auto result = ::read(_nativeHandle, buffer, maxBytes);

    if (result == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        else
            throw SystemError{};
    }

    if (result == 0)
        _endOfStream = true;

    return result;
}

std::size_t FileStream::Read(void* buffer, std::size_t maxBytes, std::chrono::milliseconds timeout) {
    struct pollfd pfd;
    pfd.fd = _nativeHandle;
    pfd.events = POLLIN;

    int result = ::poll(&pfd, 1, timeout.count());

    if (result  == -1)
        throw SystemError{};

    if (result == 0)
        throw Timeout{};

    auto bytesRead = Read(buffer, maxBytes);

    if (bytesRead == 0)
        _endOfStream = true;

    return bytesRead;
}

bool FileStream::EndOfStream() const {
    return _endOfStream;
}

std::size_t FileStream::Write(const void* buffer, std::size_t maxBytes) {
    auto bytesWritten = ::write(_nativeHandle, buffer, maxBytes);

    if (bytesWritten == -1)
        throw SystemError{};

    return bytesWritten;
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
    if (_nativeHandle != InvalidHandle)
        if (-1 == ::close(_nativeHandle))
            Log::Warning("Failed to close fd {}", _nativeHandle);
}

void SeekableFileStream::Seek(std::size_t offset) {
    if (-1 == ::lseek(_nativeHandle, 0, SEEK_SET))
        throw SystemError{};
}

} // namespace Chili

