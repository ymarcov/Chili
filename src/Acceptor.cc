#include "Acceptor.h"
#include "ExitTrap.h"
#include "Log.h"
#include "SystemError.h"

#include <algorithm>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Chili {

Acceptor::Acceptor(int listeners)
    : _listeners(listeners)
    , _listenerSockets(listeners)
    , _listenerThreads(listeners) {}

Acceptor::~Acceptor() {
    Stop();
}

std::future<void> Acceptor::Start() {
    std::lock_guard lock(_startStopMutex);

    if (!_stop || _dispatchThread.joinable())
        throw std::logic_error("Start() called when socket server is already running");

    _semaphore = std::make_unique<Semaphore>();

    for (auto& s : _listenerSockets)
        ResetListenerSocket(s);

    _stop = false;
    _promise = std::promise<void>{};

    _dispatchThread = std::thread([=] { DispatchLoop(); });

    for (auto i = 0; i < _listeners; i++)
        _listenerThreads[i] = std::thread([=] { AcceptLoop(i); });

    return _promise.get_future();
}

void Acceptor::AcceptLoop(int listener) {
    while (!_stop) {
        // block until a new connection is accepted
        int ret = ::accept(_listenerSockets[listener].GetNativeHandle(),
                           reinterpret_cast<::sockaddr*>(AddressBuffer()),
                           reinterpret_cast<::socklen_t*>(AddressBufferSize()));

        if (ret > 0) {
            std::unique_lock lock(_mutex);
            _acceptedFds.push(ret);
            lock.unlock();
            _semaphore->Increment();
            Profiler::Record<SocketQueued>();
        } else {
            if (_stop) {
                // OK: we were supposed to stop
                return;
            } else {
                // Oops: something bad happened

                auto ignored = {
                    ECONNABORTED,
                    EMFILE,
                    ENFILE,
                    ENOBUFS,
                    ENOMEM,
                    EPROTO,
                    EPERM
                };

                if (std::find(begin(ignored), end(ignored), ret) == end(ignored)) {
                    _promiseException = std::make_exception_ptr(SystemError{});
                    _stop = true;
                    Log::Error("Socket server closed due to unrecoverable error");
                    return;
                } else {
                    continue;
                }
            }
        }
    }
}

void Acceptor::DispatchLoop() {
    auto onExit = CreateExitTrap([&] {
        // all work is done. notify future.
        if (_promiseException)
            _promise.set_exception(_promiseException);
        else
            _promise.set_value();

    });

    while (!_stop) {
        _semaphore->Decrement();

        if (_stop)
            return;

        std::unique_lock lock(_mutex);

        auto fd = _acceptedFds.front();
        _acceptedFds.pop();

        lock.unlock();

        Profiler::Record<SocketDequeued>();

        try {
            RelinquishSocket(fd);
            Profiler::Record<SocketAccepted>();
        } catch (...) {
            Log::Warning("Socket server OnAccepted() threw an error which was ignored. Please handle internally!");
        }
    }
}

void Acceptor::Stop() {
    std::lock_guard lock(_startStopMutex);

    _stop = true;

    for (auto& s : _listenerSockets)
        s = SocketStream{};

    if (_semaphore)
        _semaphore->Increment();

    for (auto& t : _listenerThreads)
        if (t.joinable())
            t.join();

    if (_dispatchThread.joinable())
        _dispatchThread.join();

    _semaphore.reset();
}

std::string AcceptorEvent::GetSource() const {
    return "Acceptor";
}

std::string AcceptorEvent::GetSummary() const {
    return "Event on Acceptor";
}

void AcceptorEvent::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void SocketQueued::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void SocketDequeued::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void SocketAccepted::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

} // namespace Chili

