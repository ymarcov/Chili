#include "ThreadPool.h"

namespace Yam {
namespace Http {

class Worker {
public:
    Worker(ThreadPool* tp) :
        _tp(tp) {}

    void operator()() noexcept {
        while (!_tp->_stop)
            try { if (Fetch()) Execute(); } catch (...) {}
    }

private:
    bool Fetch() {
        std::unique_lock<std::mutex> lock(_tp->_mutex);
        _tp->_cv.wait(lock, [&] { return _tp->_stop || !_tp->_pending.empty(); });
        if (_tp->_stop) return false;
        _context = std::move(_tp->_pending.front());
        _tp->_pending.pop();
        return true;
    }

    void Execute() {
        try {
            _context->_work();
            _context->_promise.set_value();
        } catch (...) {
            _context->_promise.set_exception(std::current_exception());
        }
    }

    ThreadPool* _tp;
    std::unique_ptr<ThreadPool::WorkContext> _context;
};

ThreadPool::ThreadPool(int capacity) :
    _capacity{capacity} {
    _threads.resize(capacity);
    for (auto& t : _threads) t.reset(new std::thread{Worker{this}});
}

ThreadPool::~ThreadPool() {
    _mutex.lock();
    _stop = true;
    _mutex.unlock();
    _cv.notify_all();
    for (auto& t : _threads) t->join();
}

std::future<void> ThreadPool::Post(Work w) {
    std::future<void> result;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pending.push(std::make_unique<WorkContext>(std::move(w)));
        result = _pending.back()->_promise.get_future();
    }
    _cv.notify_one();
    return result;
}

} // namespace Http
} // namespace Yam

