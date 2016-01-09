#pragma once

#include "FileStream.h"
#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace Yam {
namespace Http {

class Poller {
public:
    enum class Registration {
        Continue,
        Conclude
    };

    enum Events {
        Shutdown = 0x1,
        Writable = 0x2,
        Readable = 0x4,
        Hangup = 0x8,
        Error  =0x10
    };

    using EventHandler = std::function<Registration(std::shared_ptr<FileStream>, int events)>;

    Poller(std::shared_ptr<ThreadPool>);
    Poller(const Poller&) = delete;
    ~Poller();

    std::size_t GetWatchedCount();
    void Register(std::shared_ptr<FileStream>);
    void Unregister(const FileStream&);

    std::future<void> Start(EventHandler);
    void Stop();

private:
    void PollLoop(const EventHandler&);
    int ConvertMask(int);

    std::shared_ptr<ThreadPool> _threadPool;
    int _fd;
    std::atomic_bool _stop;
    std::thread _thread;
    std::promise<void> _promise;
    std::map<const void*, std::shared_ptr<FileStream>> _files;
    mutable std::mutex _filesMutex;
};

} // namespace Http
} // namespace Yam

