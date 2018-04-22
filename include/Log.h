#pragma once

#include "BackTrace.h"
#include "Logger.h"

#include <atomic>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <sstream>

namespace Chili {

class Log {
public:
    enum class Level {
        Verbose,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    static void SetLevel(Level level) {
        _currentLevel = level;
    }

    static Level GetLevel() {
        return _currentLevel;
    }

    template <class... Args>
    static void Verbose(const char* format, Args&&... args) {
        if (Enabled(Level::Verbose))
            _logger->Log("Verbose", fmt::format(format, args...));
    }

    template <class... Args>
    static void Debug(const char* format, Args&&... args) {
        if (Enabled(Level::Debug))
            _logger->Log("Debug", fmt::format(format, args...));
    }

    template <class... Args>
    static void Info(const char* format, Args&&... args) {
        if (Enabled(Level::Info))
            _logger->Log("Info", fmt::format(format, args...));
    }

    template <class... Args>
    static void Warning(const char* format, Args&&... args) {
        if (Enabled(Level::Warning))
            _logger->Log("Warning", fmt::format(format, args...));
    }

    template <class... Args>
    static void Error(const char* format, Args&&... args) {
        if (Enabled(Level::Error))
            _logger->Log("Error", fmt::format(format, args...));
    }

    template <class... Args>
    [[noreturn]] static void Fatal(const char* format, Args&&... args) {
        if (Enabled(Level::Fatal)) {
            std::ostringstream msg;
            msg << fmt::format(format, args...);
            msg << "\n|--> Back trace:";
            msg << "\n|--> =======================\n";
            auto frames = BackTrace().GetFrames();
            for (auto& f : frames)
                msg << "|--> " << f << '\n';
            msg << "|--> =======================\n";
            _logger->Log("Fatal", msg.str());
        }

        std::terminate();
    }

private:
    static bool Enabled(Level level) {
        return (int)_currentLevel.load() <= (int)level;
    }

    static std::shared_ptr<Logger> _logger;
    static std::atomic<Level> _currentLevel;
};

} // namespace Chili

