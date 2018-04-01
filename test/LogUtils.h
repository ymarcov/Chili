#pragma once

#include "Log.h"

namespace Chili {

struct TemporaryLogLevel {
    TemporaryLogLevel(Log::Level level)
        : _previousLevel(Log::GetLevel()) {
        Log::SetLevel(level);
    }

    ~TemporaryLogLevel() {
    Log::SetLevel(_previousLevel);
    }

    Log::Level _previousLevel;
};

} // namespace Chili

