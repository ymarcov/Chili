#pragma once

#include "Clock.h"
#include "Semaphore.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Chili {

class ThreadPool {
public:
    using Work = std::function<void()>;

    ThreadPool(int capacity);
    ~ThreadPool();

    void Stop();
    std::future<void> Post(Work);
    std::size_t GetWorkerCount() const;

    void SetUpscalePatience(std::chrono::microseconds);
    void SetDownscalePatience(std::chrono::microseconds);

private:
    friend class Worker;

    struct WorkContext {
        WorkContext(Work&& w);

        std::chrono::nanoseconds PendingTime() const;

        std::promise<void> _promise;
        Work _work;
        Clock::TimePoint _submissionTime;
    };

    void SpawnWorker();
    bool NeedWorker() const;
    void Collect() const;

    int _capacity;
    mutable std::vector<std::shared_ptr<class Worker>> _workers;
    std::queue<std::unique_ptr<WorkContext>> _pending;
    mutable std::mutex _mutex;
    mutable bool _needToCollect{false};
    Semaphore _semaphore;
    std::chrono::microseconds _upscalePatience{500};
    std::chrono::microseconds _downscalePatience{5'000'000};
    std::atomic_bool _stop{false};
    std::thread _upscaler;
};

} // namespace Chili

