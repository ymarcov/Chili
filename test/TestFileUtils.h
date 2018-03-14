#pragma once

#include "SystemError.h"

namespace Nitra {

inline int OpenTempFile() {
    int fd = fileno(std::tmpfile());

    if (fd == -1)
        throw SystemError{};

    return fd;
}

} // namespace Nitra

