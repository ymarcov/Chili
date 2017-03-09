#pragma once

#include <string>

namespace Yam {
namespace Http {

class Logger {
public:
    virtual void Log(const char* levelTag, const std::string& message) = 0;
};

} // namespace Http
} // namespace Yam

