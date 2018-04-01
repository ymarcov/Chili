#include "Log.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace Chili {

namespace {

struct ConsoleLogger : Logger {
    void Log(const char* levelTag, const std::string& message) override {
        static std::mutex mutex;
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        tm t;
        ::localtime_r(&time, &t);
        char buffer[0x100];
        if (!std::strftime(buffer, sizeof(buffer), "%F %T", &t))
                std::strncpy(buffer, "UNKNOWN TIME", sizeof(buffer));

        std::lock_guard lock(mutex);
        std::cerr << levelTag[0] << ":[" << buffer << "] " << message << std::endl;
    }
};

} // unnamed namespace

std::shared_ptr<Logger> Log::_logger = std::make_shared<ConsoleLogger>();
std::atomic<Log::Level> Log::_currentLevel = Log::Level::Verbose;

} // namespace Chili

