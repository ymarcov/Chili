#pragma once

#include <string>

namespace Yam {
namespace Http {

class Uri {
public:
    static std::string Encode(const std::string&);
    static std::string Decode(const std::string&);
};

} // namespace Http
} // namespace Yam

