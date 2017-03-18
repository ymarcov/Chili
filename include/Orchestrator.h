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

    void Add(std::shared_ptr<FileStream>);
    void ThrottleRead(Throttler);
    void ThrottleWrite(Throttler);
    void SetInactivityTimeout(std::chrono::milliseconds);

    Signal<> OnStop;

private:
    class Task {
    public:
        void MarkHandlingInProcess(bool);
        bool IsHandlingInProcess() const;
        void Activate();
        bool ReachedInactivityTimeout() const;
        Channel& GetChannel();
        std::mutex& GetMutex();

    private:
        Orchestrator* _orchestrator;
        std::shared_ptr<Channel> _channel;
        std::chrono::time_point<std::chrono::steady_clock> _lastActive;
        std::mutex _mutex;
        std::atomic_bool _inProcess{false};

        friend void Orchestrator::Add(std::shared_ptr<FileStream>);
    };

    void OnEvent(std::shared_ptr<FileStream>, int events);
    void HandleChannelEvent(Channel&, int events);
    void IterateOnce();
    std::vector<std::shared_ptr<Task>> CaptureTasks();
    void InternalStop();
    void InternalForceStopOnError();
    bool AtLeastOneTaskIsReady();
    std::chrono::time_point<std::chrono::steady_clock> GetLatestAllowedWakeup();
    void CollectGarbage();

    std::unique_ptr<ChannelFactory> _channelFactory;
    std::promise<void> _threadPromise;
    Poller _poller;
    ThreadPool _threadPool;
    std::future<void> _pollerTask;
    std::shared_ptr<Throttler> _masterReadThrottler;
    std::shared_ptr<Throttler> _masterWriteThrottler;
    std::thread _thread;
    std::condition_variable _newEvent;
    std::atomic_bool _stop{true};
    std::mutex _mutex;
    std::vector<std::shared_ptr<Task>> _tasks;
    std::atomic<std::chrono::milliseconds> _inactivityTimeout{std::chrono::milliseconds(10000)};
};

} // namespace Http
} // namespace Yam

