#pragma once

#include "SystemError.h"

namespace Yam {
namespace Http {

inline int OpenTempFile() {
    int fd = fileno(std::tmpfile());

    if (fd == -1)
        throw SystemError{};

    return fd;
}

} // namespace Http
} // namespace Yam

