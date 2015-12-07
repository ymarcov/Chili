#pragma once

#include <cstddef>

namespace Yam {
namespace Http {

class InputStream {
public:
    virtual std::size_t Read(void*, std::size_t) = 0;
    virtual ~InputStream() = default;
};

} // namespace Http
} // namespace Yam

