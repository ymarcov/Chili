#pragma once

#include <cstddef>

namespace Yam {
namespace Http {

class OutputStream {
public:
    virtual void Write(const void*, std::size_t) = 0;
    virtual ~OutputStream() = default;
};

} // namespace Http
} // namespace Yam

