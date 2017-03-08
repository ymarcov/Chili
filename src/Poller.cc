#include "Poller.h"
#include "SystemError.h"

#include <sys/epoll.h>
#include <unistd.h>

namespace Yam {
namespace Http {

Poller::Poller(std::shared_ptr<ThreadPool> threadPool) :
    _threadPool{std::move(threadPool)},
    _stop{true} {
    if (-1 == (_fd = ::epoll_create1(EPOLL_CLOEXEC)))
        throw SystemError{};
}

Poller::~Poller() {
    Stop();
    ::close(_fd);
}

std::size_t Poller::GetWatchedCount() {
    std::lock_guard<std::mutex> lock{_filesMutex};
    return _files.size();
}

void Poller::Poll(std::shared_ptr<FileStream> fs, int events) {
    InsertOrIncrementRefCount(fs, events);
}

// Handling ref counts makes it possible to re-register
// from within a handler, even though straight after
// it the dispatcher will decrement the ref count.
void Poller::InsertOrIncrementRefCount(std::shared_ptr<FileStream>& fs, int events) {
    struct epoll_event ev;

    ev.events = EPOLLONESHOT | ConvertToNative(events);
    ev.data.ptr = fs.get();

    std::lock_guard<std::mutex> lock{_filesMutex};

    auto it = _files.find(fs.get());

    if (it == end(_files)) {
        _files[fs.get()] = std::make_pair(1, fs);

        if (-1 == ::epoll_ctl(_fd, EPOLL_CTL_ADD, fs->GetNativeHandle(), &ev)) {
            _files.erase(fs.get());
            throw SystemError{};
        }
    } else {
        ++_files[fs.get()].first;

        if (-1 == ::epoll_ctl(_fd, EPOLL_CTL_MOD, fs->GetNativeHandle(), &ev)) {
            if (-1 == ::epoll_ctl(_fd, EPOLL_CTL_DEL, fs->GetNativeHandle(), nullptr))
                ; // TODO: log this?

            _files.erase(fs.get());
            throw SystemError{};
        }
    }

}

void Poller::DecrementRefCount(const FileStream& fs) {
    std::lock_guard<std::mutex> lock{_filesMutex};

    auto it = _files.find(&fs);

    if (it != _files.end()) {
        auto streamFd = fs.GetNativeHandle();

        if (it->second.first == 1) {
            _files.erase(it);
        } else {
            --it->second.first;
            return;
        }

        if (::epoll_ctl(_fd, EPOLL_CTL_DEL, streamFd, nullptr))
            ; // TODO Log this!

    }
}

std::future<void> Poller::Start(EventHandler handler) {
    if (!_stop || _thread.joinable())
        throw std::logic_error("Poller started while already running");

    _promise = std::promise<void>{};
    _stop = false;
    _thread = std::thread([=] { PollLoop(handler); });

    return _promise.get_future();
}

void Poller::Stop() {
    _stop = true;

    if (_thread.joinable())
        _thread.join();
}

void Poller::PollLoop(const Poller::EventHandler& handler) {
    constexpr int maxEvents = 100;
    struct epoll_event events[maxEvents];
    int n;

    while (!_stop) {
        if (-1 == (n = ::epoll_wait(_fd, events, maxEvents, 100))) {
            _promise.set_exception(std::make_exception_ptr(SystemError{}));
            OnStop();
            return;
        }

        DispatchEvents(events, n, handler);
    }

    _promise.set_value();
    OnStop();
}

void Poller::DispatchEvents(void* eventsPtr, std::size_t n, const Poller::EventHandler& handler) {
    auto events = static_cast<struct epoll_event*>(eventsPtr);

    for (std::size_t i = 0; i < n; ++i) {
        auto eventMask = events[i].events;
        auto fs = GetFileStreamFromPtr(events[i].data.ptr);

        if (!fs) {
            // TODO: log?
            continue;
        }

        _threadPool->Post([=] {
            try {
                handler(fs, ConvertFromNative(eventMask));
            } catch (...) {
            }

            DecrementRefCount(*fs);
        });
    }
}

std::shared_ptr<FileStream> Poller::GetFileStreamFromPtr(void* ptr) {
    std::lock_guard<std::mutex> lock{_filesMutex};
    auto it = _files.find(ptr);
    return it == _files.end() ? nullptr : it->second.second;
}

int Poller::ConvertFromNative(int m) {
    int result = 0;

    if (m & EPOLLIN)
        result |= Events::Readable;

    if (m & EPOLLOUT)
        result |= Events::Writable;

    if (m & EPOLLRDHUP)
        result |= Events::EndOfStream;

    if (m & EPOLLHUP)
        result |= Events::Hangup;

    if (m & EPOLLERR)
        result |= Events::Error;

    return result;
}

int Poller::ConvertToNative(int m) {
    int result = 0;

    if (m & Events::Readable)
        result |= EPOLLIN;

    if (m & Events::Writable)
        result |= EPOLLOUT;

    if (m & Events::EndOfStream)
        result |= EPOLLRDHUP;

    if (m & Events::Hangup)
        result |= EPOLLHUP;

    if (m & Events::Error)
        result |= EPOLLERR;

    return result;
}

} // namespace Http
} // namespace Yam

