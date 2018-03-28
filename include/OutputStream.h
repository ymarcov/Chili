#pragma once

#include <cstddef>

namespace Chili {

class OutputStream {
public:
    virtual std::size_t Write(const void*, std::size_t) = 0;
    virtual ~OutputStream() = default;
};

} // namespace Chili

