#include "Orchestrator.h"

#include <algorithm>

namespace Yam {
namespace Http {

void Orchestrator::Task::Activate() {
    GetChannel().PerformStage();
    _lastActive = std::chrono::steady_clock::now();
}

bool Orchestrator::Task::ReachedInactivityTimeout() const {
    auto diff = std::chrono::steady_clock::now() - _lastActive;
    return diff >= *_inactivityTimeout;
}

Channel& Orchestrator::Task::GetChannel() {
    return *_channel;
}

Orchestrator::Orchestrator(std::unique_ptr<ChannelFactory> channelFactory) :
    _channelFactory(std::move(channelFactory)),
    _poller(2),
    _masterReadThrottler(std::make_shared<Throttler>()),
    _masterWriteThrottler(std::make_shared<Throttler>()) {
    _poller.OnStop += [this] {
        _stop = true;
        _cv.notify_one();
    };
}

Orchestrator::~Orchestrator() {
    Stop();
}

std::future<void> Orchestrator::Start() {
    _stop = false;
    _threadPromise = std::promise<void>();

    _thread = std::thread([this] {
        try {
            while (!_stop)
                IterateOnce();

            _poller.Stop();
            _pollerTask.wait();
            OnStop();

            try {
                _pollerTask.get();
                _threadPromise.set_value();
            } catch (...) {
                _threadPromise.set_exception(std::current_exception());
            }
        } catch (...) {
            _stop = true;
            _poller.Stop();
            _pollerTask.wait();
            OnStop();

            try {
                _pollerTask.get();
                _threadPromise.set_exception(std::current_exception());
            } catch (...) {
                _threadPromise.set_exception(std::current_exception());
            }
        }
    });

    _pollerTask = _poller.Start([this](std::shared_ptr<FileStream> fs, int events) {
        return OnEvent(std::move(fs), events);
    });

    return _threadPromise.get_future();
}

void Orchestrator::Stop() {
    _stop = true;
    _cv.notify_one();

    if (_thread.joinable())
        _thread.join();
}

std::shared_ptr<Channel> Orchestrator::Add(std::shared_ptr<FileStream> stream) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto task = std::make_shared<Task>();
    auto throttlers = Channel::Throttlers();

    throttlers.Read.Master = _masterReadThrottler;
    throttlers.Write.Master = _masterWriteThrottler;

    task->_channel = _channelFactory->CreateChannel(std::move(stream), std::move(throttlers));
    task->_lastActive = std::chrono::steady_clock::now();
    task->_inactivityTimeout = &_inactivityTimeout;

    _tasks.push_back(task);

    _poller.Poll(task->GetChannel().GetStream(), Poller::Events::Completion | Poller::Events::Readable);

    return task->_channel;
}

void Orchestrator::OnEvent(std::shared_ptr<FileStream> fs, int events) {
    std::unique_lock<std::mutex> lock(_mutex);

    auto task = std::find_if(begin(_tasks), end(_tasks), [&](auto& t) {
        return t->GetChannel().GetStream().get() == fs.get();
    });

    if (task != end(_tasks)) {
        auto& channel = (*task)->GetChannel();

        if (events & Poller::Events::Completion) {
            // TODO: log this?
            channel.Close();
            return;
        }

        switch (channel.GetStage()) {
            case Channel::Stage::WaitReadable:
                if (events & Poller::Events::Readable)
                    channel.SetStage(Channel::Stage::Read);
                else
                    ; // TODO: log this? seems like a logic error
                break;

            case Channel::Stage::WaitWritable:
                if (events & Poller::Events::Writable)
                    channel.SetStage(Channel::Stage::Write);
                else
                    ; // TODO: log this? seems like a logic error
                break;

            case Channel::Stage::Closed:
                return;

            default:
                // TODO: log this? seems like a logic error
                channel.Close();
                return;
        }

        lock.unlock();
        _cv.notify_one();
    } else {
        // TODO: log this?
    }
}

void Orchestrator::IterateOnce() {
    for (auto& task : CaptureTasks()) {
        if (_stop)
            return;

        if (task->ReachedInactivityTimeout()) {
            task->GetChannel().Close();
        } else if (task->GetChannel().IsReady()) {
            task->Activate();

            switch (task->GetChannel().GetStage()) {
                case Channel::Stage::WaitReadable:
                    _poller.Poll(task->GetChannel().GetStream(), Poller::Events::Completion | Poller::Events::Readable);
                    break;

                case Channel::Stage::WaitWritable:
                    _poller.Poll(task->GetChannel().GetStream(), Poller::Events::Completion | Poller::Events::Writable);
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

    auto isReady = [this] {
        return _stop || (end(_tasks) != std::find_if(begin(_tasks), end(_tasks), [this](auto& t) {
            return t->GetChannel().IsReady();
        }));
    };

    auto timeout = std::chrono::steady_clock::now() + _inactivityTimeout;

    for (auto& t : _tasks) {
        auto& channel = t->GetChannel();

        if (channel.IsWaiting()) {
            auto&& requestedTimeout = channel.GetRequestedTimeout();

            if (requestedTimeout < timeout)
                timeout = requestedTimeout;
        }
    }

    if (timeout > std::chrono::steady_clock::now())
        _cv.wait_until(lock, timeout, isReady);

    // Garbage-collect closed tasks
    _tasks.erase(std::remove_if(begin(_tasks), end(_tasks), [](auto& t) {
        return t->GetChannel().GetStage() == Channel::Stage::Closed;
    }), end(_tasks));

    auto snapshot = _tasks;

    return snapshot;
}

} // namespace Http
} // namespace Yam

