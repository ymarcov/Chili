#pragma once

#include "FileStream.h"
#include "Profiler.h"
#include "Signal.h"
#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace Nitra {

class Poller {
public:
    enum Events {
        EndOfStream = 0x1,
        Writable = 0x2,
        Readable = 0x4,
        Hangup = 0x8,
        Error  = 0x10,
        Completion = EndOfStream | Hangup | Error,
        NotifyAll = EndOfStream | Writable | Readable
    };

    using EventHandler = std::function<void(std::shared_ptr<FileStream>, int events)>;

    Poller(int threads);
    Poller(const Poller&) = delete;
    ~Poller();

    std::size_t GetWatchedCount();
    void Poll(std::shared_ptr<FileStream>, int events = Events::NotifyAll);
    void Remove(const std::shared_ptr<FileStream>&);

    std::future<void> Start(EventHandler);
    void Stop();

    Signal<> OnStop;

private:
    void InsertOrIncrementRefCount(std::shared_ptr<FileStream>&, int events);
    void DecrementRefCount(const FileStream&);
    void PollLoop(const EventHandler&);
    void DispatchEvents(void* events, std::size_t n, const EventHandler&);
    std::shared_ptr<FileStream> GetFileStreamFromPtr(void*);
    int ConvertFromNative(int);
    int ConvertToNative(int);

    std::shared_ptr<ThreadPool> _threadPool;
    int _fd;
    std::atomic_bool _stop;
    std::thread _thread;
    std::promise<void> _promise;
    std::map<const void*, std::pair<unsigned, std::shared_ptr<FileStream>>> _files;
    std::mutex _filesMutex;
};

/**
 * Profiling
 */

class PollerEvent : public ProfileEvent {
public:
    std::string GetSource() const override;
    std::string GetSummary() const override;
    void Accept(ProfileEventReader&) const override;
};

class PollerEventDispatched : public PollerEvent {
public:
    using PollerEvent::PollerEvent;
    void Accept(ProfileEventReader&) const override;
};

} // namespace Nitra

