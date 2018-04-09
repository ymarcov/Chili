#include "ThreadPool.h"

using namespace std::literals;

namespace Chili {

class Worker {
public:
    Worker(ThreadPool* tp) :
        _threadPool{tp} {
        _thread = std::thread([this] {
            Run();
        });
    }

    ~Worker() {
        Join();
    }

    void Join() {
        std::lock_guard lock(_mutex);

        if (_thread.joinable())
            _thread.join();
    }

    bool IsAlive() const {
        return _isAlive;
    }

    void Run() noexcept {
        while (Fetch())
            Execute();
    }

private:
    bool Fetch() {
        std::unique_lock lock(_threadPool->_mutex);

        auto patience = _threadPool->_downscalePatience;

        lock.unlock();

        if (!_threadPool->_semaphore.TryDecrement(patience)) {
            _isAlive = false;
            lock.lock();
            _threadPool->_needToCollect = true;
            return false;
        }

        lock.lock();

        if (_threadPool->_stop)
            return false;

        _workContext = std::move(_threadPool->_pending.front());
        _threadPool->_pending.pop();

        return true;
    }

    void Execute() {
        auto wc = std::move(_workContext);

        try {
            wc->_work();
            wc->_promise.set_value();
        } catch (...) {
            wc->_promise.set_exception(std::current_exception());
        }
    }

    ThreadPool* _threadPool;
    std::thread _thread;
    std::mutex _mutex;
    std::atomic_bool _isAlive{true};
    std::unique_ptr<ThreadPool::WorkContext> _workContext;
};

ThreadPool::WorkContext::WorkContext(Work&& w)
    : _work(std::move(w))
    , _submissionTime(Clock::GetCurrentTime()) {}

std::chrono::nanoseconds ThreadPool::WorkContext::PendingTime() const {
    return Clock::GetCurrentTime() - _submissionTime;
}

ThreadPool::ThreadPool(int capacity)
    : _capacity(capacity) {
    if (_capacity < 1)
        throw std::logic_error("ThreadPool capacity must be at least 1");
}

ThreadPool::~ThreadPool() {
    Stop();
}


std::size_t ThreadPool::GetWorkerCount() const {
    std::lock_guard lock(_mutex);
    Collect();
    return _workers.size();
}

void ThreadPool::Collect() const {
    if (!_needToCollect)
        return;

    for (auto i = begin(_workers); i != end(_workers);) {
        if (!(*i)->IsAlive())
            i = _workers.erase(i);
        else
            ++i;
    }

    _needToCollect = false;
}

void ThreadPool::SetUpscalePatience(std::chrono::microseconds p) {
    std::lock_guard lock(_mutex);
    _upscalePatience = p;
}

void ThreadPool::SetDownscalePatience(std::chrono::microseconds p) {
    std::lock_guard lock(_mutex);
    _downscalePatience = p;
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
    for (auto& w : _workers)
        w->Join();
}

std::future<void> ThreadPool::Post(Work w) {
    auto workContext = std::make_unique<WorkContext>(std::move(w));
    auto workFuture = workContext->_promise.get_future();

    {
        // lock queue and push work item
        std::lock_guard<std::mutex> lock(_mutex);

        if (_stop)
            return {};

        if (NeedWorker())
            SpawnWorker();

        _pending.push(std::move(workContext));
    }

    // explicitly wake up one sleeping worker.
    _semaphore.Increment();

    return workFuture;
}

void ThreadPool::SpawnWorker() {
    _workers.push_back(std::make_unique<Worker>(this));
}

bool ThreadPool::NeedWorker() const {
    Collect();

    if (_capacity == _workers.size())
        return false;

    if (_workers.empty())
        return true;

    if (_pending.empty())
        return false;

    if(_pending.front()->PendingTime() > _upscalePatience)
        return true;

    return false;
}

} // namespace Chili

