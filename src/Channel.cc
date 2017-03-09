#include "Channel.h"

namespace Yam {
namespace Http {

Channel::Channel(std::shared_ptr<FileStream> stream, Throttlers throttlers) :
    _stream(std::move(stream)),
    _throttlers(std::move(throttlers)),
    _request(_stream),
    _responder(_stream),
    _timeout(std::chrono::steady_clock::now()),
    _stage(Stage::WaitReadable) {
}

Channel::~Channel() {}

void Channel::PerformStage() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try {
        switch (_stage) {
            case Stage::WaitReadTimeout:
            case Stage::Read:
                OnRead();
                break;

            case Stage::Process:
                OnProcess();
                break;

            case Stage::WaitWriteTimeout:
            case Stage::Write:
                OnWrite();
                break;

            default:
                throw std::logic_error("PerformStage() called in non-ready stage");
        }
    } catch (const std::exception&) {
        // TODO: log?
        Close();
    }
}

Channel::Stage Channel::GetStage() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _stage;
}

void Channel::SetStage(Stage s) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _stage = s;
}

std::chrono::time_point<std::chrono::steady_clock> Channel::GetRequestedTimeout() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _timeout;
}

bool Channel::IsReady() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (std::chrono::steady_clock::now() < _timeout)
        return false;

    return (_stage != Stage::WaitReadable) &&
            (_stage != Stage::WaitWritable) &&
            (_stage != Stage::Closed);
}

bool Channel::IsWaiting() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    if (std::chrono::steady_clock::now() < _timeout)
        return true;

    return (_stage == Stage::WaitReadable) ||
            (_stage == Stage::WaitWritable);
}

void Channel::OnRead() {
    auto maxRead = std::min(_throttlers.Read.Dedicated.GetCurrentQuota(),
                            _throttlers.Read.Master->GetCurrentQuota());

    if (maxRead == 0) {
        _stage = Stage::WaitReadTimeout;
        _timeout = _throttlers.Read.Dedicated.GetFillTime();
        return;
    }

    if (_fetchContent) {
        auto result = _request.ConsumeContent(maxRead);

        _throttlers.Read.Dedicated.Consume(result.second);
        _throttlers.Read.Master->Consume(result.second);

        if (!result.first) {
            if (result.second <= maxRead) {
                _stage = Stage::WaitReadable;
            } else {
                _stage = Stage::WaitReadTimeout;
                _timeout = _throttlers.Read.Dedicated.GetFillTime();
            }

            return;
        }
    } else {
        auto result = _request.ConsumeHeader(maxRead);

        _throttlers.Read.Dedicated.Consume(result.second);
        _throttlers.Read.Master->Consume(result.second);

        if (!result.first) {
            if (result.second <= maxRead) {
                _stage = Stage::WaitReadable;
            } else {
                _stage = Stage::WaitReadTimeout;
                _timeout = _throttlers.Read.Dedicated.GetFillTime();
            }

            return;
        }
    }

    _responder = Responder(_stream);
    _stage = Stage::Process;
}

void Channel::OnProcess() {
    Control result;
    _stage = Stage::WaitWritable;

    try {
        result = Process();
    } catch (...) {
        // TODO: log?
        _error = true;
        _responder = Responder(_stream);
        _responder.Send(Status::InternalServerError);
        return;
    }

    if (result == Control::FetchContent) {
        _fetchContent = true;
        _stage = Stage::Read;
    }
}

Request& Channel::GetRequest() {
    return _request;
}

Responder& Channel::GetResponder() {
    return _responder;
}

Channel::Control Channel::FetchContent() {
    return Control::FetchContent;
}

Channel::Control Channel::SendResponse(Status status) {
    _responder.Send(status);
    return Control::SendResponse;
}

void Channel::ThrottleRead(Throttler t) {
    _throttlers.Read.Dedicated = std::move(t);
}

void Channel::ThrottleWrite(Throttler t) {
    _throttlers.Write.Dedicated = std::move(t);
}

void Channel::OnWrite() {
    auto maxWrite = std::min(_throttlers.Write.Dedicated.GetCurrentQuota(),
                             _throttlers.Write.Master->GetCurrentQuota());

    if (maxWrite == 0) {
        _stage = Stage::WaitWriteTimeout;
        _timeout = _throttlers.Write.Dedicated.GetFillTime();
        return;
    }

    auto result = _responder.Flush(maxWrite);

    _throttlers.Write.Dedicated.Consume(result.second);
    _throttlers.Write.Master->Consume(result.second);

    if (!result.first) {
        if (result.second <= maxWrite) {
            _stage = Stage::WaitWritable;
        } else {
            _stage = Stage::WaitWriteTimeout;
            _timeout = _throttlers.Write.Dedicated.GetFillTime();
        }
    } else {
        if (_error || !_responder.GetKeepAlive()) {
            Close();
        } else {
            _request = Request(_stream);
            _fetchContent = false;
            _stage = Stage::WaitReadable;
        }
    }
}

void Channel::Close() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _stage = Stage::Closed;
    _timeout = std::chrono::steady_clock::now();
    _request = Request();
    _responder = Responder();
    _stream.reset();
}

const std::shared_ptr<FileStream>& Channel::GetStream() const {
    return _stream;
}

} // namespace Http
} // namespace Yam
