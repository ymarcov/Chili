#include "ThreadPool.h"

namespace Nitra {

class Worker {
public:
    Worker(ThreadPool* tp) :
        _threadPool{tp} {}

    void operator()() noexcept {
        do try {
            Fetch() && Execute();
        } catch (...) {} while (!_threadPool->_stop);
    }

private:
    bool Fetch() {
        std::unique_lock<std::mutex> lock(_threadPool->_mutex);

        // release the threadpool lock and wait until the condition variable
        // is notified. reacquire only if there's work to do, or we need to stop.

        _threadPool->_workerAlarm.wait(lock, [&] {
            return _threadPool->_stop || !_threadPool->_pending.empty();
        });

        if (_threadPool->_stop)
            return false;

        // we've really got some work to do.
        // pop it and set current work context.

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

ThreadPool::ThreadPool(int capacity) {
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
    _mutex.unlock();

    // wake everybody up now in order
    // to make sure they get the message
    // and finish their loops accordingly.
    _workerAlarm.notify_all();

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
        _pending.push(std::move(workContext));
    }

    // explicitly wake up one sleeping worker.
    // (spurious wakeup could happen, but we can't rely on it)
    _workerAlarm.notify_one();

    return workFuture;
}

} // namespace Nitra

