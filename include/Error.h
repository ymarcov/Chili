#pragma once

#include "BackTrace.h"

namespace Chili {

class Error {
public:
    const BackTrace& GetBackTrace() const {
        return _backTrace;
    }

private:
    BackTrace _backTrace;
};

} // namespace Chili

