#pragma once

#include <string>

namespace Chili {

class Uri {
public:
    static std::string Encode(const std::string&);
    static std::string Decode(const std::string&);
};

} // namespace Chili

