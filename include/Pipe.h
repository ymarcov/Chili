#pragma once

#include "FileStream.h"
#include "SystemError.h"

#include <memory>
#include <fcntl.h>
#include <unistd.h>

namespace Chili {

struct Pipe {
    static auto Create() {
        int fds[2];

        if (::pipe(fds))
            throw SystemError();

        return Pipe{std::make_shared<FileStream>(fds[0]),
                    std::make_shared<FileStream>(fds[1])};
    }

    std::shared_ptr<FileStream> Read;
    std::shared_ptr<FileStream> Write;
};

} // namespace Chili

