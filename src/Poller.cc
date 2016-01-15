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

void Poller::Register(std::shared_ptr<FileStream> fs) {
    struct epoll_event ev;

    ev.events = EPOLLET | EPOLLIN | EPOLLHUP | EPOLLRDHUP;
    ev.data.ptr = fs.get();

    std::lock_guard<std::mutex> lock{_filesMutex};

    _files[fs.get()] = fs;

    if (-1 == ::epoll_ctl(_fd, EPOLL_CTL_ADD, fs->GetNativeHandle(), &ev)) {
        _files.erase(fs.get());
        throw SystemError{};
    }

    ++_fdCount;
}

void Poller::Unregister(const FileStream& fs) {
    std::lock_guard<std::mutex> lock{_filesMutex};

    auto it = _files.find(&fs);

    /*
     * We can enter here twice if epoll
     * shot more than one event, which
     * queued more than one task which
     * either requested to conclude the
     * registration, or errored out.
     */
    if (it != _files.end()) {
        auto streamFd = fs.GetNativeHandle();

        _files.erase(it);

        if (!::epoll_ctl(_fd, EPOLL_CTL_DEL, streamFd, nullptr))
            --_fdCount;
        else
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

    while (!_stop) {
        int result;

        while (-1 == (result = ::epoll_wait(_fd, events, maxEvents, 100)))
            if (errno != EINTR)
                break;

        if (result == -1) {
            _promise.set_exception(std::make_exception_ptr(SystemError{}));
            OnStop();
            return;
        }

        DispatchEvents(events, result, handler);
    }

    _promise.set_value();
    OnStop();
}

void Poller::DispatchEvents(void* eventsPtr, std::size_t n, const Poller::EventHandler& handler) {
    auto events = static_cast<struct epoll_event*>(eventsPtr);

    for (std::size_t i = 0; i < n; ++i) {
        auto eventMask = events[i].events;
        auto fs = GetFileStreamFromPtr(events[i].data.ptr);

        _threadPool->Post([=] {
            Registration r;

            try {
                r = handler(fs, ConvertMask(eventMask));
            } catch (...) {
                r = Registration::Conclude;
            }

            if (r == Registration::Conclude)
                Unregister(*fs);
        });
    }
}

std::shared_ptr<FileStream> Poller::GetFileStreamFromPtr(void* ptr) {
    std::lock_guard<std::mutex> lock{_filesMutex};
    auto it = _files.find(ptr);
    return it == _files.end() ? nullptr : it->second;
}

int Poller::ConvertMask(int m) {
    int result = 0;

    if (m & EPOLLIN)
        result |= Events::Readable;

    if (m & EPOLLOUT)
        result |= Events::Writable;

    if (m & EPOLLRDHUP)
        result |= Events::Shutdown;

    if (m & EPOLLHUP)
        result |= Events::Hangup;

    if (m & EPOLLERR)
        result |= Events::Error;

    return result;
}

} // namespace Http
} // namespace Yam

