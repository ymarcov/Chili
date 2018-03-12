#pragma once

#include <string>

namespace Nitra {

class Uri {
public:
    static std::string Encode(const std::string&);
    static std::string Decode(const std::string&);
};

} // namespace Nitra

