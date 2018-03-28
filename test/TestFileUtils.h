#pragma once

#include "SystemError.h"

namespace Chili {

inline int OpenTempFile() {
    int fd = fileno(std::tmpfile());

    if (fd == -1)
        throw SystemError{};

    return fd;
}

} // namespace Chili

