#include "Orchestrator.h"

#include <algorithm>

namespace Yam {
namespace Http {

std::chrono::milliseconds Orchestrator::Task::Inactivity() const {
    auto diff = std::chrono::steady_clock::now() - _lastActive;
    return std::chrono::duration_cast<std::chrono::milliseconds>(diff);
}

Orchestrator::Orchestrator(std::unique_ptr<ChannelFactory> channelFactory) :
    _channelFactory(std::move(channelFactory)),
    _poller(std::make_shared<ThreadPool>(2)),
    _masterReadThrottler(std::make_shared<Throttler>()),
    _masterWriteThrottler(std::make_shared<Throttler>()) {
    _poller.OnStop += [this] { OnStop(); };
}

Orchestrator::~Orchestrator() {
    Stop();
}

std::future<void> Orchestrator::Start() {
    _stop = false;

    _thread = std::thread([this] {
        try {
            std::unique_lock<std::mutex> lock(_mutex);

            while (!_stop) {
                _cv.wait_for(lock, _inactivityTimeout);
                lock.unlock();
                IterateOnce();
            }
        } catch (const std::exception& e) {
            _stop = true;
            _poller.Stop();
        }
    });

    return _poller.Start([this](std::shared_ptr<FileStream> fs, int events) {
        return OnEvent(std::move(fs), events);
    });
}

void Orchestrator::Stop() {
    _stop = true;

    _cv.notify_one();

    if (_thread.joinable())
        _thread.join();

    _poller.Stop();
}

std::shared_ptr<Channel> Orchestrator::Add(std::shared_ptr<FileStream> stream) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto task = std::make_shared<Task>();
    auto throttlers = Channel::Throttlers();

    throttlers.Read.Master = _masterReadThrottler;
    throttlers.Write.Master = _masterWriteThrottler;

    task->_channel = _channelFactory->CreateChannel(std::move(stream), std::move(throttlers));
    task->_lastActive = std::chrono::steady_clock::now();

    _tasks.push_back(task);

    _poller.Poll(task->_channel->GetStream(), Poller::Events::Completion | Poller::Events::Readable);

    return task->_channel;
}

void Orchestrator::OnEvent(std::shared_ptr<FileStream> fs, int events) {
    std::unique_lock<std::mutex> lock(_mutex);

    auto task = std::find_if(begin(_tasks), end(_tasks), [&](auto& t) {
        return t->_channel->GetStream().get() == fs.get();
    });

    if (task != end(_tasks)) {
        auto& channel = (*task)->_channel;

        lock.unlock();

        if (events & Poller::Events::Completion) {
            // TODO: log this?
            channel->Close();
            return;
        }

        switch (channel->GetStage()) {
            case Channel::Stage::WaitReadable:
                if (events & Poller::Events::Readable)
                    channel->SetStage(Channel::Stage::Read);
                else
                    ; // TODO: log this? seems like a logic error
                break;

            case Channel::Stage::WaitWritable:
                if (events & Poller::Events::Writable)
                    channel->SetStage(Channel::Stage::Write);
                else
                    ; // TODO: log this? seems like a logic error
                break;

            default:
                // TODO: log this? seems like a logic error
                channel->Close();
                return;
        }

        _cv.notify_one();
    } else {
        // TODO: log this?
    }
}

void Orchestrator::IterateOnce() {
    for (auto& task : CaptureTasks()) {
        if (_stop)
            return;

        if (task->Inactivity() >= _inactivityTimeout) {
            task->_channel->Close();
        } else if (task->_channel->IsReady()) {
            task->_channel->PerformStage();
            task->_lastActive = std::chrono::steady_clock::now();

            switch (task->_channel->GetStage()) {
                case Channel::Stage::WaitReadable:
                    _poller.Poll(task->_channel->GetStream(), Poller::Events::Completion | Poller::Events::Readable);
                    break;

                case Channel::Stage::WaitWritable:
                    _poller.Poll(task->_channel->GetStream(), Poller::Events::Completion | Poller::Events::Writable);
                    break;

                default:
                    // No need to re-poll at this point
                    break;
            }
        }
    }
}

std::vector<std::shared_ptr<Orchestrator::Task>> Orchestrator::CaptureTasks() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto minTimeout = std::min_element(begin(_tasks), end(_tasks), [this](auto& t1, auto& t2) {
        return t1->_channel->GetTimeout() < t2->_channel->GetTimeout();
    });

    auto isReady = [this] {
        return _stop || (end(_tasks) != std::find_if(begin(_tasks), end(_tasks), [this](auto& t) {
            return t->_channel->IsReady();
        }));
    };

    if (minTimeout != end(_tasks))
        _cv.wait_until(lock, (*minTimeout)->_channel->GetTimeout(), isReady);
    else
        _cv.wait(lock, isReady);

    // Garbage-collect closed tasks
    _tasks.erase(std::remove_if(begin(_tasks), end(_tasks), [](auto& t) {
        return t->_channel->GetStage() == Channel::Stage::Closed;
    }));

    return _tasks;
}

} // namespace Http
} // namespace Yam

