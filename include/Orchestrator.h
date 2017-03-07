#pragma once

#include "Channel.h"
#include "FileStream.h"
#include "Poller.h"
#include "Signal.h"
#include "Throttler.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

namespace Yam {
namespace Http {

class Orchestrator {
public:
    Orchestrator();
    ~Orchestrator();

    std::future<void> Start();
    void Stop();

    std::shared_ptr<Channel> Add(std::shared_ptr<FileStream>, Throttler write = {}, Throttler read = {});

    Signal<> OnStop;

private:
    struct Task {
        std::shared_ptr<Channel> _channel;
        std::chrono::time_point<std::chrono::steady_clock> _lastActive;

        std::chrono::milliseconds Inactivity() const;
    };

    void OnEvent(std::shared_ptr<FileStream>, int events);
    void IterateOnce();
    std::vector<std::shared_ptr<Task>> CaptureTasks();

    Poller _poller;
    std::shared_ptr<Throttler> _masterReadThrottler;
    std::shared_ptr<Throttler> _masterWriteThrottler;
    std::thread _thread;
    std::condition_variable _cv;
    std::atomic_bool _stop{true};
    std::mutex _mutex;
    std::vector<std::shared_ptr<Task>> _tasks;
    std::chrono::milliseconds _inactivityTimeout;
};

} // namespace Http
} // namespace Yam

