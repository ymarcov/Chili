#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Yam {
namespace Http {

class ThreadPool {
public:
    using Work = std::function<void()>;

    ThreadPool(int capacity);
    ~ThreadPool();

    void Stop();
    std::future<void> Post(Work);
    std::size_t GetThreadCount() const;

private:
    friend class Worker;

    struct WorkContext {
        WorkContext(Work&& w) : _work(std::move(w)) {}
        std::promise<void> _promise;
        Work _work;
    };

    std::vector<std::unique_ptr<std::thread>> _threads;
    std::queue<std::unique_ptr<WorkContext>> _pending;
    std::mutex _mutex;
    std::condition_variable _workerAlarm;
    std::atomic_bool _stop{false};
};

inline std::size_t ThreadPool::GetThreadCount() const {
    return _threads.size();
}

} // namespace Http
} // namespace Yam

