#pragma once

#include "BackTrace.h"
#include "Logger.h"

#include <atomic>
#include <fmtlib/format.h>
#include <sstream>

namespace Yam {
namespace Http {

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

    /**
     * Gets the default log.
     */
    static Log* Default();

    Log(std::shared_ptr<Logger> logger, Level level = Level::Verbose) :
        _logger(std::move(logger)),
        _currentLevel(level) {}

    void SetLevel(Level level) {
        _currentLevel = level;
    }

    template <class... Args>
    void Verbose(const char* format, Args&&... args) {
        if (Enabled(Level::Verbose))
            _logger->Log("Verbose", fmt::format(format, args...));
    }

    template <class... Args>
    void Debug(const char* format, Args&&... args) {
        if (Enabled(Level::Debug))
            _logger->Log("Debug", fmt::format(format, args...));
    }

    template <class... Args>
    void Info(const char* format, Args&&... args) {
        if (Enabled(Level::Info))
            _logger->Log("Info", fmt::format(format, args...));
    }

    template <class... Args>
    void Warning(const char* format, Args&&... args) {
        if (Enabled(Level::Warning))
            _logger->Log("Warning", fmt::format(format, args...));
    }

    template <class... Args>
    void Error(const char* format, Args&&... args) {
        if (Enabled(Level::Error))
            _logger->Log("Error", fmt::format(format, args...));
    }

    template <class... Args>
    void Fatal(const char* format, Args&&... args) {
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
            std::terminate();
        }
    }

private:
    bool Enabled(Level level) const {
        return (int)_currentLevel.load() <= (int)level;
    }

    std::shared_ptr<Logger> _logger;
    std::atomic<Level> _currentLevel;
};

} // namespace Http
} // namespace Yam

