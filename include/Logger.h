#pragma once

#include <string>

namespace Chili {

class Logger {
public:
    virtual void Log(const char* levelTag, const std::string& message) = 0;
};

} // namespace Chili

