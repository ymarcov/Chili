#include "Channel.h"
#include "Log.h"

#include <atomic>

namespace Yam {
namespace Http {

std::atomic<std::uint64_t> nextChannelId{0};

Channel::Channel(std::shared_ptr<FileStream> stream, Throttlers throttlers) :
    _id(++nextChannelId),
    _stream(std::move(stream)),
    _throttlers(std::move(throttlers)),
    _request(_stream),
    _responder(_stream),
    _timeout(std::chrono::steady_clock::now()),
    _stage(Stage::WaitReadable) {
    Log::Default()->Verbose("Channel {} created", _id);
}

Channel::~Channel() {}

void Channel::PerformStage() {
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
    } catch (const std::exception& e) {
        Log::Default()->Info("Channel {} PerformStage() error: {}", _id, e.what());
        Close();
    }
}

Channel::Stage Channel::GetStage() const {
    return _stage;
}

void Channel::SetStage(Stage s) {
    if (_stage == Stage::Closed)
        return;

    _stage = s;
}

std::chrono::time_point<std::chrono::steady_clock> Channel::GetRequestedTimeout() const {
    return _timeout;
}

bool Channel::IsReady() const {
    if (std::chrono::steady_clock::now() < _timeout)
        return false;

    return (_stage != Stage::WaitReadable) &&
            (_stage != Stage::WaitWritable) &&
            (_stage != Stage::Closed);
}

bool Channel::IsWaiting() const {
    if (std::chrono::steady_clock::now() < _timeout)
        return true;

    return (_stage == Stage::WaitReadable) ||
            (_stage == Stage::WaitWritable);
}

void Channel::OnRead() {
    auto maxRead = std::min(_throttlers.Read.Dedicated.GetCurrentQuota(),
                            _throttlers.Read.Master->GetCurrentQuota());

    if (maxRead == 0) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
        _stage = Stage::WaitReadTimeout;
        _timeout = _throttlers.Read.Dedicated.GetFillTime();
        return;
    }

    if (_fetchContent) {
        auto result = _request.ConsumeContent(maxRead);

        _throttlers.Read.Dedicated.Consume(result.second);
        _throttlers.Read.Master->Consume(result.second);

        if (!result.first) {
            if (result.second < maxRead) {
                Log::Default()->Verbose("Channel {} socket buffer empty. Waiting for readability.", _id);
                _stage = Stage::WaitReadable;
            } else {
                Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
                _stage = Stage::WaitReadTimeout;
                _timeout = _throttlers.Read.Dedicated.GetFillTime();
            }

            return;
        } else {
            _fetchContent = false;
        }
    } else {
        auto result = _request.ConsumeHeader(maxRead);

        _throttlers.Read.Dedicated.Consume(result.second);
        _throttlers.Read.Master->Consume(result.second);

        if (!result.first) {
            if (result.second < maxRead) {
                Log::Default()->Verbose("Channel {} socket buffer empty. Waiting for readability.", _id);
                _stage = Stage::WaitReadable;
            } else {
                Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
                _stage = Stage::WaitReadTimeout;
                _timeout = _throttlers.Read.Dedicated.GetFillTime();
            }

            return;
        } else {
            std::string method, version;

            switch (_request.GetMethod()) {
                case Method::Head:
                    method = "HEAD";
                    break;

                case Method::Get:
                    method = "GET";
                    break;

                case Method::Post:
                    method = "POST";
                    break;

                default:
                    Log::Default()->Info("Unsupported method for {}! Dropping request on channel {}.", _request.GetUri(), _id);
                    break;
            }

            switch (_request.GetVersion()) {
                case Version::Http10:
                    version = "HTTP/1.0";
                    break;

                case Version::Http11:
                    version = "HTTP/1.1";
                    break;
            }

            Log::Default()->Info("Channel {} Received \"{} {} {}\"", _id, method, _request.GetUri(), version);
        }
    }

    _responder = Responder(_stream);
    _stage = Stage::Process;
}

void Channel::OnProcess() {
    Control result;
    _stage = Stage::Write;

    try {
        result = Process();
    } catch (...) {
        Log::Default()->Error("Channel {} processor error ignored! Please handle internally.", _id);
        _error = true;
        _responder = Responder(_stream);
        _responder.Send(Status::InternalServerError);
        return;
    }

    if (result == Control::FetchContent) {
        if (_fetchContent) {
            Log::Default()->Error("Channel {} processor asked for content to be fetched twice! Closing!", _id);
            Close();
            return;
        }

        _fetchContent = true;
        std::string value;

        if (_request.GetField("Expect", &value)) {
            if (value == "100-continue") {
                _responder = Responder(_stream);
                _responder.Send(Status::Continue);
            }
        } else {
            _stage = Stage::Read;
        }
    } else if (result == Control::RejectContent) {
        std::string value;

        if (_request.GetField("Expect", &value)) {
            if (value == "100-continue") {
                _responder = Responder(_stream);
                _responder.Send(Status::ExpectationFailed);
            }
        } else {
            Close();
        }
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

Channel::Control Channel::RejectContent() {
    return Control::RejectContent;
}

Channel::Control Channel::SendResponse(Status status) {
    _responder.Send(status);
    return Control::SendResponse;
}

bool Channel::IsReadThrottled() const {
    return _throttlers.Read.Dedicated.IsEnabled();
}

bool Channel::IsWriteThrottled() const {
    return _throttlers.Write.Dedicated.IsEnabled();
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
        Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
        _stage = Stage::WaitWriteTimeout;
        _timeout = _throttlers.Write.Dedicated.GetFillTime();
        return;
    }

    auto result = _responder.Flush(maxWrite);

    _throttlers.Write.Dedicated.Consume(result.second);
    _throttlers.Write.Master->Consume(result.second);

    if (!result.first) {
        if (result.second < maxWrite) {
            Log::Default()->Verbose("Channel {} socket buffer full. Waiting for writability.", _id);
            _stage = Stage::WaitWritable;
        } else {
            Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
            _stage = Stage::WaitWriteTimeout;
            _timeout = _throttlers.Write.Dedicated.GetFillTime();
        }
    } else {
        if (_fetchContent) {
            _stage = Stage::Read;
        } else if (_error || !_responder.GetKeepAlive()) {
            if (_error)
                Log::Default()->Verbose("Channel {} encountered an error and sent an error response", _id);
            else
                Log::Default()->Verbose("Channel {} sent response and closing", _id);
            Close();
        } else {
            Log::Default()->Verbose("Channel {} sent response and keeps alive", _id);
            _request = Request(_stream);
            _fetchContent = false;
            _stage = Stage::Read;
        }
    }
}

void Channel::Close() {
    if (_stage == Stage::Closed)
        return;

    Log::Default()->Verbose("Channel {} closed", _id);

    _stage = Stage::Closed;
    _timeout = std::chrono::steady_clock::now();
    _request = Request();
    _responder = Responder();
    _stream.reset();
}

const std::shared_ptr<FileStream>& Channel::GetStream() const {
    return _stream;
}

int Channel::GetId() const {
    return _id;
}

} // namespace Http
} // namespace Yam
