#pragma once

#include "Log.h"

namespace Yam {
namespace Http {

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

} // namespace Http
} // namespace Yam

