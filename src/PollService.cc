#include "PollService.h"

#include "Log.h"

namespace Yam {
namespace Http {

PollService::PollService(int threads) :
    _poller(threads) {
    _poller.Start([=](std::shared_ptr<FileStream> fs, int events) {
        std::unique_lock<std::mutex> lock(_mutex);

        auto it = _tasks.find(fs.get());

        if (it == _tasks.end())
            Log::Default()->Fatal("FileStream triggered in PollService has no task");

        auto task = std::move(it->second);

        _tasks.erase(it);

        lock.unlock();

        task._handler(task._file, events);

        task._promise.set_value(std::move(task._file));
    });
}

PollService::~PollService() {
    _poller.Stop();
}

std::future<std::shared_ptr<FileStream>> PollService::Poll(std::shared_ptr<FileStream> fs,
                                                           Poller::EventHandler handler,
                                                           int events) {
    std::unique_lock<std::mutex> lock(_mutex);
    auto result = _tasks.emplace(std::make_pair(fs.get(), Task{fs, handler}));
    lock.unlock();

    auto& task = result.first->second;

    _poller.Poll(task._file, events);

    return task._promise.get_future();
}

} // namespace Http
} // namespace Yam

