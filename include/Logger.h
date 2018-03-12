#pragma once

#include <string>

namespace Nitra {

class Logger {
public:
    virtual void Log(const char* levelTag, const std::string& message) = 0;
};

} // namespace Nitra

