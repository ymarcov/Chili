#pragma once

#include "ChannelBase.h"
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

namespace Nitra {

class Orchestrator {
public:
    Orchestrator(std::shared_ptr<ChannelFactory>, int threads);
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
        ChannelBase& GetChannel();
        std::mutex& GetMutex();

    private:
        Orchestrator* _orchestrator;
        std::shared_ptr<ChannelBase> _channel;
        std::chrono::time_point<std::chrono::steady_clock> _lastActive;
        std::mutex _mutex;
        mutable std::mutex _lastActiveMutex;
        std::atomic_bool _inProcess{false};

        friend void Orchestrator::Add(std::shared_ptr<FileStream>);
    };

    void OnEvent(std::shared_ptr<FileStream>, int events);
    void HandleChannelEvent(ChannelBase&, int events);
    void IterateOnce();
    std::vector<std::shared_ptr<Task>> CaptureTasks();
    std::vector<std::shared_ptr<Task>> FilterReadyTasks();
    void InternalStop();
    void InternalForceStopOnError();
    bool AtLeastOneTaskIsReady();
    bool IsTaskReady(Task&);
    std::chrono::time_point<std::chrono::steady_clock> GetLatestAllowedWakeup();
    void CollectGarbage();

    std::shared_ptr<ChannelFactory> _channelFactory;
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

} // namespace Nitra

