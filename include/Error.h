#pragma once

#include "BackTrace.h"

namespace Yam {
namespace Http {

class Error {
public:
    const BackTrace& GetBackTrace() const {
        return _backTrace;
    }

private:
    BackTrace _backTrace;
};

} // namespace Http
} // namespace Yam

