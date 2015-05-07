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

    std::future<void> Post(Work);

private:
    friend class Worker;

    struct WorkContext {
        WorkContext(Work&& w) : _work(std::move(w)) {}
        std::promise<void> _promise;
        Work _work;
    };

    int _capacity;
    std::vector<std::unique_ptr<std::thread>> _threads;
    std::queue<std::unique_ptr<WorkContext>> _pending;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic_bool _stop{false};
};

} // namespace Http
} // namespace Yam

