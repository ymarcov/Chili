#pragma once

#include "Log.h"

namespace Nitra {

struct TemporaryLogLevel {
    TemporaryLogLevel(Log::Level level, Log* log = Log::Default()) :
        _log(log),
        _previousLevel(log->GetLevel()) {
        _log->SetLevel(level);
    }

    ~TemporaryLogLevel() {
        _log->SetLevel(_previousLevel);
    }

    Log* _log;
    Log::Level _previousLevel;
};

} // namespace Nitra

