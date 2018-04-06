#include "ThreadPool.h"

namespace Chili {

class Worker {
public:
    Worker(ThreadPool* tp) :
        _threadPool{tp} {}

    void operator()() noexcept {
        while (Fetch())
            Execute();
    }

private:
    bool Fetch() {
        _threadPool->_semaphore.Decrement();

        std::lock_guard lock(_threadPool->_mutex);

        if (_threadPool->_stop)
            return false;

        _workContext = std::move(_threadPool->_pending.front());
        _threadPool->_pending.pop();

        return true;
    }

    bool Execute() {
        auto wc = std::move(_workContext);

        try {
            wc->_work();
            wc->_promise.set_value();
            return true;
        } catch (...) {
            wc->_promise.set_exception(std::current_exception());
            return false;
        }
    }

    ThreadPool* _threadPool;
    std::unique_ptr<ThreadPool::WorkContext> _workContext;
};

ThreadPool::ThreadPool(int capacity)
    : _capacity(capacity) {
    // create workers
    _threads.resize(capacity);
    for (auto& t : _threads)
        t.reset(new std::thread{Worker{this}});
}

ThreadPool::~ThreadPool() {
    Stop();
}

void ThreadPool::Stop() {
    // change the stop flag to true
    _mutex.lock();

    _stop = true;

    while (!_pending.empty())
        _pending.pop();

    _mutex.unlock();

    // wake everybody up now in order
    // to make sure they get the message
    // and finish their loops accordingly.
    for (auto i = 0; i < _capacity; ++i)
        _semaphore.Increment();

    // wait for everyone to finish
    for (auto& t : _threads)
        if (t->joinable())
            t->join();
}

std::future<void> ThreadPool::Post(Work w) {
    auto workContext = std::make_unique<WorkContext>(std::move(w));
    auto workFuture = workContext->_promise.get_future();

    {
        // lock queue and push work item
        std::lock_guard<std::mutex> lock(_mutex);

        if (_stop)
            return {};

        _pending.push(std::move(workContext));
    }

    // explicitly wake up one sleeping worker.
    _semaphore.Increment();

    return workFuture;
}

} // namespace Chili

