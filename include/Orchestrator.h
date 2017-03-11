#pragma once

#include "Channel.h"
#include "ChannelFactory.h"
#include "FileStream.h"
#include "Poller.h"
#include "Signal.h"
#include "ThreadPool.h"
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
    Orchestrator(std::unique_ptr<ChannelFactory>, int threads);
    ~Orchestrator();

    std::future<void> Start();
    void Stop();

    std::shared_ptr<Channel> Add(std::shared_ptr<FileStream>);
    void ThrottleRead(Throttler);
    void ThrottleWrite(Throttler);

    Signal<> OnStop;

private:
    struct Task {
        std::shared_ptr<Channel> _channel;
        std::chrono::time_point<std::chrono::steady_clock> _lastActive;
        std::chrono::milliseconds* _inactivityTimeout;
        std::mutex _mutex;

        void Activate();
        bool ReachedInactivityTimeout() const;
        Channel& GetChannel();
        std::mutex& GetMutex();
    };

    void OnEvent(std::shared_ptr<FileStream>, int events);
    void IterateOnce();
    std::vector<std::shared_ptr<Task>> CaptureTasks();

    std::unique_ptr<ChannelFactory> _channelFactory;
    std::promise<void> _threadPromise;
    Poller _poller;
    ThreadPool _threadPool;
    std::future<void> _pollerTask;
    std::shared_ptr<Throttler> _masterReadThrottler;
    std::shared_ptr<Throttler> _masterWriteThrottler;
    std::thread _thread;
    std::condition_variable _cv;
    std::atomic_bool _stop{true};
    std::mutex _mutex;
    std::vector<std::shared_ptr<Task>> _tasks;
    std::chrono::milliseconds _inactivityTimeout{10000};
};

} // namespace Http
} // namespace Yam

