#include "Socket.h"
#include "SystemError.h"

#include <algorithm>
#include <cerrno>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace Yam {
namespace Http {

Socket::Socket(Socket::NativeHandle fd) :
    _fd{fd},
    _null{false} {
    if (_fd == -1)
        throw std::logic_error("Invalid socket");
}

Socket::Socket(Socket&& s) :
    _fd{-1},
    _null{true} {
    std::swap(_fd, s._fd);
    std::swap(_null, s._null);
}

Socket::~Socket() {
    if (_null)
        return;

    Close();
}

void Socket::Close() {
    ::shutdown(_fd, SHUT_RDWR);

    while (-1 == ::close(_fd))
        if (errno != EINTR)
            return; // TODO log with SystemError
}

Socket& Socket::operator=(Socket&& s) {
    Close();

    _fd = -1;
    _null = true;
    std::swap(_fd, s._fd);
    std::swap(_null, s._null);

    return *this;
}

std::size_t Socket::Read(void* buffer, std::size_t bufferSize) {
    if (_null)
        throw std::logic_error("Read operation on null socket object");

    ::ssize_t result;

    while (-1 == (result = ::recv(_fd, buffer, bufferSize, 0)))
        if (errno != EINTR)
            throw SystemError{};

    return result;
}

std::size_t Socket::Write(const void* buffer, std::size_t bufferSize) {
    if (_null)
        throw std::logic_error("Read operation on null socket object");

    ::ssize_t result;

    while (-1 == (result = ::send(_fd, buffer, bufferSize, MSG_NOSIGNAL)))
        if (errno != EINTR)
            throw SystemError{};

    return result;
}

} // namespace Http
} // namespace Yam

