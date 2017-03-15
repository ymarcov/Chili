#include "Orchestrator.h"
#include "Log.h"

#include <algorithm>

namespace Yam {
namespace Http {

void Orchestrator::Task::Activate() {
    auto& channel = GetChannel();

    if (ReachedInactivityTimeout()) {
        Log::Default()->Info("Channel {} reached inactivity timeout", channel.GetId());
        _orchestrator->_poller.Remove(channel.GetStream());
        channel.Close();
        _pending = false;
        _orchestrator->_newEvent.notify_one();
        return;
    }

    if (!channel.IsReady()) {
        _pending = false;
        return;
    }

    channel.Advance();

    _pending = false;

    switch (channel.GetStage()) {
        case Channel::Stage::Process:
            _lastActive = std::chrono::steady_clock::now();
        case Channel::Stage::Read:
        case Channel::Stage::ReadTimeout:
        case Channel::Stage::Write:
        case Channel::Stage::WriteTimeout:
        case Channel::Stage::Closed:
            _orchestrator->_newEvent.notify_one();
            break;

        case Channel::Stage::WaitReadable: {
            _orchestrator->_poller.Poll(channel.GetStream(),
                                        Poller::Events::Completion | Poller::Events::Readable);
        } break;

        case Channel::Stage::WaitWritable: {
            _orchestrator->_poller.Poll(channel.GetStream(),
                                        Poller::Events::Completion | Poller::Events::Writable);
        } break;

        default:
            break; // Don't need to get involved
    }

}

bool Orchestrator::Task::ReachedInactivityTimeout() const {
    auto diff = std::chrono::steady_clock::now() - _lastActive;
    return diff >= _orchestrator->_inactivityTimeout;
}

Channel& Orchestrator::Task::GetChannel() {
    return *_channel;
}

std::mutex& Orchestrator::Task::GetMutex() {
    return _mutex;
}

Orchestrator::Orchestrator(std::unique_ptr<ChannelFactory> channelFactory, int threads) :
    _channelFactory(std::move(channelFactory)),
    _poller(2),
    _threadPool(threads),
    _masterReadThrottler(std::make_shared<Throttler>()),
    _masterWriteThrottler(std::make_shared<Throttler>()) {
    _poller.OnStop += [this] {
        _stop = true;
        _newEvent.notify_one();
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

            InternalStop();
        } catch (...) {
            InternalForceStopOnError();
        }
    });

    _pollerTask = _poller.Start([this](std::shared_ptr<FileStream> fs, int events) {
        return OnEvent(std::move(fs), events);
    });

    return _threadPromise.get_future();
}

void Orchestrator::InternalStop() {
    try {
        _poller.Stop();
        _threadPool.Stop();
        OnStop();
        _pollerTask.get();
        _threadPromise.set_value();
    } catch (...) {
        _threadPromise.set_exception(std::current_exception());
    }
}

void Orchestrator::InternalForceStopOnError() {
    Log::Default()->Error("Orchestrator stopped due to error!");

    try {
        _stop = true;
        _poller.Stop();
        _threadPool.Stop();
        OnStop();
        _pollerTask.get();
        _threadPromise.set_exception(std::current_exception());
    } catch (...) {
        _threadPromise.set_exception(std::current_exception());
    }
}

void Orchestrator::Stop() {
    std::unique_lock<std::mutex> lock(_mutex);
    _stop = true;
    lock.unlock();
    _newEvent.notify_one();

    if (_thread.joinable())
        _thread.join();
}

std::shared_ptr<Channel> Orchestrator::Add(std::shared_ptr<FileStream> stream) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto task = std::make_shared<Task>();
    auto throttlers = Channel::Throttlers();

    throttlers.Read.Master = _masterReadThrottler;
    throttlers.Write.Master = _masterWriteThrottler;

    task->_orchestrator = this;
    task->_channel = _channelFactory->CreateChannel(std::move(stream), std::move(throttlers));
    task->_lastActive = std::chrono::steady_clock::now();

    _tasks.push_back(task);

    _poller.Poll(task->GetChannel().GetStream(), Poller::Events::Completion | Poller::Events::Readable);

    return task->_channel;
}

void Orchestrator::ThrottleRead(Throttler t) {
    *_masterReadThrottler = std::move(t);
}

void Orchestrator::ThrottleWrite(Throttler t) {
    *_masterWriteThrottler = std::move(t);
}

void Orchestrator::OnEvent(std::shared_ptr<FileStream> fs, int events) {
    std::unique_lock<std::mutex> lock(_mutex);

    auto it = std::find_if(begin(_tasks), end(_tasks), [&](auto& t) {
        return t->GetChannel().GetStream().get() == fs.get();
    });

    if (it == end(_tasks))
        return;

    auto task = *it;

    lock.unlock();

    {
        std::lock_guard<std::mutex> taskLock(task->GetMutex());
        HandleChannelEvent(task->GetChannel(), events);
    }

    _newEvent.notify_one();
}

void Orchestrator::HandleChannelEvent(Channel& channel, int events) {
    if (events & Poller::Events::Completion) {
        Log::Default()->Verbose("Channel {} received completion event", channel.GetId());
        channel.Close();
        return;
    }

    switch (channel.GetStage()) {
        case Channel::Stage::WaitReadable: {
            if (events & Poller::Events::Readable) {
                Log::Default()->Verbose("Channel {} became readable", channel.GetId());
                channel.SetStage(Channel::Stage::Read);
            } else {
                Log::Default()->Error("Channel {} was waiting for readbility but got different "
                                      "event. Check poll logic!", channel.GetId());
            }
        } break;

        case Channel::Stage::WaitWritable: {
            if (events & Poller::Events::Writable) {
                Log::Default()->Verbose("Channel {} became writable", channel.GetId());
                channel.SetStage(Channel::Stage::Write);
            } else {
                Log::Default()->Error("Channel {} was waiting for writability but got different "
                                      "event. Check poll logic!", channel.GetId());
            }
        } break;

        case Channel::Stage::Closed: {
            Log::Default()->Verbose("Ignoring event on already closed channel {}", channel.GetId());
            return;
         }

        default: {
            Log::Default()->Error("Channel {} was not in a waiting stage but received an event. "
                                  "Check poll logic!", channel.GetId());
            channel.Close();
            return;
         }
    }
}

void Orchestrator::IterateOnce() {
    for (auto& task : CaptureTasks()) {
        if (_stop)
            break;

        if (!task->_pending) {
            task->_pending = true;

            _threadPool.Post([=] {
                std::lock_guard<std::mutex> lock(task->GetMutex());
                task->Activate();
            });
        }
    }
}

std::vector<std::shared_ptr<Orchestrator::Task>> Orchestrator::CaptureTasks() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto timeout = GetLatestAllowedWakeup();

    if (timeout > std::chrono::steady_clock::now()) {
        _newEvent.wait_until(lock, timeout, [&] {
            timeout = GetLatestAllowedWakeup();
            return _stop || AtLeastOneTaskIsReady();
        });
    }

    CollectGarbage();

    auto snapshot = _tasks;

    return snapshot;
}

bool Orchestrator::AtLeastOneTaskIsReady() {
    return end(_tasks) != std::find_if(begin(_tasks), end(_tasks), [this](auto& t) {
        return !t->_pending && t->GetChannel().IsReady();
    });
}

std::chrono::time_point<std::chrono::steady_clock> Orchestrator::GetLatestAllowedWakeup() {
    auto now = std::chrono::steady_clock::now();
    auto timeout = now + _inactivityTimeout;

    for (auto& t : _tasks) {
        auto& channel = t->GetChannel();
        auto&& requestedTimeout = channel.GetRequestedTimeout();

        if ((requestedTimeout > now) && (requestedTimeout < timeout))
            timeout = requestedTimeout;
    }

    return timeout;
}

void Orchestrator::CollectGarbage() {
    _tasks.erase(std::remove_if(begin(_tasks), end(_tasks), [](auto& t) {
        // For some reason adding the mutex here, though I'm not sure
        // it's even necessary correctness-wise, makes things much faster.
        std::lock_guard<std::mutex> lock(t->GetMutex());
        return t->GetChannel().GetStage() == Channel::Stage::Closed;
    }), end(_tasks));
}

} // namespace Http
} // namespace Yam

