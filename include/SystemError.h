#pragma once

#include "Error.h"

#include <cerrno>
#include <system_error>

namespace Yam {
namespace Http {

class SystemError : public Error, public std::system_error {
public:
    SystemError() :
        system_error(errno, std::system_category()) {}

    std::string GetMessage() const {
        return what();
    }
};

} // namespace Http
} // namespace Yam

