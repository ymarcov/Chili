#pragma once

#include "BackTrace.h"

#include <cerrno>
#include <system_error>

namespace Yam {
namespace Http {

class SystemError : public std::system_error {
public:
    SystemError() :
        system_error(errno, std::system_category()) {}

    std::string GetMessage() const {
        return what();
    }

    const BackTrace& GetBackTrace() const {
        return _backTrace;
    }

private:
    BackTrace _backTrace;
};

} // namespace Http
} // namespace Yam

