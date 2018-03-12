#pragma once

#include "BackTrace.h"

namespace Nitra {

class Error {
public:
    const BackTrace& GetBackTrace() const {
        return _backTrace;
    }

private:
    BackTrace _backTrace;
};

} // namespace Nitra

