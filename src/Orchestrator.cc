#include "Orchestrator.h"
#include "Log.h"

#include <algorithm>

namespace Yam {
namespace Http {

void Orchestrator::Task::MarkHandlingInProcess(bool b) {
    _inProcess = b;
}

bool Orchestrator::Task::IsHandlingInProcess() const {
    return _inProcess;
}

void Orchestrator::Task::Activate() {
    if (ReachedInactivityTimeout()) {
        Log::Default()->Info("Channel {} reached inactivity timeout", _channel->GetId());

        // If it happened while it was in the poller, then remove
        // it from there as well. Otherwise, this call should be
        // okay with us trying to remove a non-existent channel.
        _orchestrator->_poller.Remove(_channel->GetStream());

        _channel->Close();
        _inProcess = false;
        _orchestrator->_newEvent.notify_one();
        return;
    }

    // Money line
    _channel->Advance();

    {
        // This is checked from other places in parallel via
        // ReachedInactivityTimeout(). So we need to lock it.
        std::lock_guard<std::mutex> lock(_lastActiveMutex);
        _lastActive = std::chrono::steady_clock::now();
    }

    // In case we're sending it off to the poller,
    // we don't need to notify our main thread,
    // because the task won't be ready until it
    // comes back from the poller with an event.
    bool notify = false;

    switch (_channel->GetStage()) {
        case AbstractChannel::Stage::WaitReadable: {
            _orchestrator->_poller.Poll(_channel->GetStream(),
                                        Poller::Events::Completion | Poller::Events::Readable);
        } break;

        case AbstractChannel::Stage::WaitWritable: {
            _orchestrator->_poller.Poll(_channel->GetStream(),
                                        Poller::Events::Completion | Poller::Events::Writable);
        } break;

        default:
            // Ok, it's not going to the poller.
            // It's ready for its next stage already.
            // Therefore, wake up our main thread
            // so that it could schedule its next
            // stage whenever it sees fit.
            notify = true;
            break;
    }

    // Now it makes sense to be rescheduled again,
    // either immediately, or when we come back
    // from the poller with an event.
    _inProcess = false;

    if (notify) {
        // Wake up our main thread so that our
        // next stage can be scheduled.
        _orchestrator->_newEvent.notify_one();
    }
}

bool Orchestrator::Task::ReachedInactivityTimeout() const {
    if (!_channel->IsWaitingForClient()) {
        // can't blame the client, we just
        // haven't got to handling it yet
        return false;
    }

    decltype(_lastActive) lastActive;

    {
        std::lock_guard<std::mutex> lock(_lastActiveMutex);
        lastActive = _lastActive;
    }

    auto diff = std::chrono::steady_clock::now() - lastActive;
    return diff >= _orchestrator->_inactivityTimeout.load();
}

AbstractChannel& Orchestrator::Task::GetChannel() {
    return *_channel;
}

std::mutex& Orchestrator::Task::GetMutex() {
    return _mutex;
}

Orchestrator::Orchestrator(std::shared_ptr<ChannelFactory> channelFactory, int threads) :
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

void Orchestrator::Add(std::shared_ptr<FileStream> stream) {
    auto task = std::make_shared<Task>();

    task->_orchestrator = this;
    task->_channel = _channelFactory->CreateChannel(std::move(stream));
    task->_channel->_throttlers.Read.Master = _masterReadThrottler;
    task->_channel->_throttlers.Write.Master = _masterWriteThrottler;
    task->_lastActive = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.push_back(task);
    }

    _poller.Poll(task->GetChannel().GetStream(), Poller::Events::Completion | Poller::Events::Readable);
}

void Orchestrator::ThrottleRead(Throttler t) {
    *_masterReadThrottler = std::move(t);
}

void Orchestrator::ThrottleWrite(Throttler t) {
    *_masterWriteThrottler = std::move(t);
}

void Orchestrator::SetInactivityTimeout(std::chrono::milliseconds ms) {
    _inactivityTimeout = ms;
}

void Orchestrator::OnEvent(std::shared_ptr<FileStream> fs, int events) {
    // Lock the shared task list state, because we're
    // going to try to find the relevant task for the
    // triggered file stream in it.
    std::unique_lock<std::mutex> lock(_mutex);

    auto it = std::find_if(begin(_tasks), end(_tasks), [&](auto& t) {
        return t->GetChannel().GetStream().get() == fs.get();
    });

    if (it == end(_tasks))
        return;

    auto task = *it;

    // Got our task. We can release the lock.
    lock.unlock();

    auto& channel = task->GetChannel();

    if (events & Poller::Events::Completion) {
        // No use talking to a wall. Even if we had other events,
        // no one's going to be listening to our replies.
        Log::Default()->Verbose("Channel {} received completion event", channel.GetId());
        channel.RequestClose();
    } else {
        std::lock_guard<std::mutex> taskLock(task->GetMutex());
        HandleChannelEvent(channel, events);
    }

    // Either way, we need to react to what just happened,
    // either by garbage-collection or by advancing the
    // relevant task's state-machine. So we need to wake
    // our main thread up to do the work.
    _newEvent.notify_one();
}

void Orchestrator::HandleChannelEvent(AbstractChannel& channel, int events) {
    switch (channel.GetStage()) {
        case AbstractChannel::Stage::WaitReadable: {
            if (events & Poller::Events::Readable) {
                Log::Default()->Verbose("Channel {} became readable", channel.GetId());
                channel.SetStage(AbstractChannel::Stage::Read);
            } else {
                Log::Default()->Error("Channel {} was waiting for readbility but got different "
                                      "event. Check poll logic!", channel.GetId());
            }
        } break;

        case AbstractChannel::Stage::WaitWritable: {
            if (events & Poller::Events::Writable) {
                Log::Default()->Verbose("Channel {} became writable", channel.GetId());
                channel.SetStage(AbstractChannel::Stage::Write);
            } else {
                Log::Default()->Error("Channel {} was waiting for writability but got different "
                                      "event. Check poll logic!", channel.GetId());
            }
        } break;

        case AbstractChannel::Stage::Closed: {
            // I'm not sure this can happen, but I'm not taking any chances.
            // At any rate, log it so that maybe it'll help understand if
            // and when it *does* happen.
            Log::Default()->Verbose("Ignoring event on already closed channel {}", channel.GetId());
            return;
         }

        default: {
            // The client is not supposed to be in the poller
            // if it wasn't waiting for anything... This must
            // be caused by a programming error.
            Log::Default()->Error("Channel {} was not in a waiting stage but received an event. "
                                  "Check poll logic!", channel.GetId());
            channel.Close();
            return;
         }
    }
}

void Orchestrator::IterateOnce() {
    for (auto& task : CaptureTasks()) {
        // Exit ASAP if server needs to stop,
        // don't wait for the next call.
        if (_stop)
            break;

        // Mark it ias being handled right here so that we
        // don't need to wait for the thread to get to it.
        // This way, the next call of CaptureTasks()
        // will filter this task out for us.
        task->MarkHandlingInProcess(true);

        _threadPool.Post([=] {
            std::lock_guard<std::mutex> lock(task->GetMutex());
            task->Activate();
        });
    }
}

std::vector<std::shared_ptr<Orchestrator::Task>> Orchestrator::CaptureTasks() {
    // We capture ready tasks into a new vector so that
    // the original vector would be released, in terms
    // of locks and mutexes, and new tasks could be
    // added to it even while we're processing tasks
    // that are already ready to be processed.
    // This way, we don't take the lock for too long.
    std::unique_lock<std::mutex> lock(_mutex);

    auto timeout = GetLatestAllowedWakeup();

    if (timeout > std::chrono::steady_clock::now()) {
        _newEvent.wait_until(lock, timeout, [&] {
            // Should the server stop?
            if (_stop)
                return true;

            // We may return and do some work
            if (AtLeastOneTaskIsReady())
                return true;

            // Recalculate our new timeout based
            // on our new circumstances.
            timeout = GetLatestAllowedWakeup();
            return false;
        });
    }

    CollectGarbage();

    return FilterReadyTasks();
}

std::vector<std::shared_ptr<Orchestrator::Task>> Orchestrator::FilterReadyTasks() {
    auto snapshot = std::vector<std::shared_ptr<Orchestrator::Task>>();
    snapshot.reserve(_tasks.size());

    std::copy_if(begin(_tasks), end(_tasks), back_inserter(snapshot), [this](auto& t) {
        return this->IsTaskReady(*t);
    });

    return snapshot;
}

bool Orchestrator::AtLeastOneTaskIsReady() {
    return end(_tasks) != std::find_if(begin(_tasks), end(_tasks), [this](auto& t) {
        return this->IsTaskReady(*t);
    });
}

bool Orchestrator::IsTaskReady(Task& t) {
    // Although the task is in our list,
    // it is in fact currently being
    // processed by some thread. So
    // we don't need to do anything
    // extra about it for now.
    if (t.IsHandlingInProcess())
        return false;

    // If close is only *requested*, it's
    // actually the task itself that needs
    // to handle its close request. So
    // we need to advance it.
    if (t.GetChannel().IsCloseRequested())
        return true;

    // If the task has reached its inactivity
    // timeout, it has to close itself, by itself.
    if (t.ReachedInactivityTimeout())
        return true;

    // Finally, is it ready for some
    // actual happy-path processing?
    return t.GetChannel().IsReady();
}

std::chrono::time_point<std::chrono::steady_clock> Orchestrator::GetLatestAllowedWakeup() {
    auto now = std::chrono::steady_clock::now();

    // Our latest possible timeout (i.e. default)
    // if nothing else is requested, is in fact
    // our inactivity timeout, when we check
    // if any channels have remained inactive
    // for too long, in which case we close them.
    auto timeout = now + _inactivityTimeout.load();

    for (auto& t : _tasks) {
        auto& channel = t->GetChannel();
        auto&& requestedTimeout = channel.GetRequestedTimeout();

        if ((requestedTimeout > now) && (requestedTimeout < timeout)) {
            // This client has requested an earlier timeout than
            // the one we were going to use. In order that we can
            // respond to its event as quickly as possible, we'll
            // take its request.
            timeout = requestedTimeout;
        }
    }

    return timeout;
}

void Orchestrator::CollectGarbage() {
    _tasks.erase(std::remove_if(begin(_tasks), end(_tasks), [](auto& t) {
        return t->GetChannel().GetStage() == AbstractChannel::Stage::Closed;
    }), end(_tasks));
}

} // namespace Http
} // namespace Yam

