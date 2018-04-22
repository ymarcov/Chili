#pragma once

#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Chili {

class OutputStream {
public:
    virtual std::size_t Write(const void*, std::size_t) = 0;
    virtual std::size_t WriteVector(std::vector<std::pair<const void*, std::size_t>>);
    virtual ~OutputStream() = default;
};

inline std::size_t OutputStream::WriteVector(std::vector<std::pair<const void*, std::size_t>> vec) {
    std::ostringstream oss;

    for (auto& [data, len] : vec)
        oss << std::string(static_cast<const char*>(data), len);

    auto result = oss.str();

    return Write(result.data(), result.size());
}

} // namespace Chili

