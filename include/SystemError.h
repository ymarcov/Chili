#pragma once

#include <cerrno>
#include <system_error>

namespace Yam {
namespace Http {

class SystemError : public std::system_error {
public:
    SystemError() :
        system_error(errno, std::system_category()) {}
};

} // namespace Http
} // namespace Yam

