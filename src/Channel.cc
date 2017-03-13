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

void Channel::Advance() {
    try {
        switch (_stage) {
            case Stage::ReadTimeout:
            case Stage::Read:
                OnRead();
                break;

            case Stage::Process:
                OnProcess();
                break;

            case Stage::WriteTimeout:
            case Stage::Write:
                OnWrite();
                break;

            default:
                throw std::logic_error("Advance() called in non-ready stage");
        }
    } catch (const std::exception& e) {
        Log::Default()->Info("Channel {} Advance() error: {}", _id, e.what());
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
    if (std::chrono::steady_clock::now() < _timeout.load())
        return false;

    return (_stage != Stage::WaitReadable) &&
            (_stage != Stage::WaitWritable) &&
            (_stage != Stage::Closed);
}

void Channel::OnRead() {
    auto maxRead = std::min(_throttlers.Read.Dedicated.GetCurrentQuota(),
                            _throttlers.Read.Master->GetCurrentQuota());

    if (maxRead == 0) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
        _stage = Stage::ReadTimeout;
        _timeout = _throttlers.Read.Dedicated.GetFillTime();
        return;
    }

    bool doneReading;

    if (_fetchingContent) {
        if ((doneReading = FetchData(&Request::ConsumeContent, maxRead)))
            _fetchingContent = false;
    } else {
        if ((doneReading = FetchData(&Request::ConsumeHeader, maxRead)))
            LogNewRequest();
    }

    if (doneReading) {
        _responder = Responder(_stream);
        _stage = Stage::Process;
    }
}

bool Channel::FetchData(std::pair<bool, std::size_t>(Request::*func)(std::size_t), std::size_t maxRead) {
    bool done;
    std::size_t bytesConsumed;

    std::tie(done, bytesConsumed) = (_request.*func)(maxRead);

    _throttlers.Read.Dedicated.Consume(bytesConsumed);
    _throttlers.Read.Master->Consume(bytesConsumed);

    if (!done) {
        if (bytesConsumed < maxRead) {
            Log::Default()->Verbose("Channel {} socket buffer empty. Waiting for readability.", _id);
            _stage = Stage::WaitReadable;
        } else {
            Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
            _stage = Stage::ReadTimeout;
            _timeout = _throttlers.Read.Dedicated.GetFillTime();
        }
    }

    return done;
}

void Channel::LogNewRequest() {
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

void Channel::OnProcess() {
    try {
        auto directive = Process();
        HandleControlDirective(directive);
    } catch (...) {
        SendInternalError();
        return;
    }
}

void Channel::SendInternalError() {
    Log::Default()->Error("Channel {} processor error ignored! Please handle internally.", _id);
    _respondedWithError = true;
    _responder = Responder(_stream);
    _responder.Send(Status::InternalServerError);
    _stage = Stage::Write;
}

void Channel::HandleControlDirective(Control directive) {
    switch (directive) {
        case Control::SendResponse:
            _stage = Stage::Write;
            break;

        case Control::FetchContent: {
            _fetchingContent = true;
            std::string value;

            if (_request.GetField("Expect", &value)) {
                if (value == "100-continue") {
                    _responder = Responder(_stream);
                    _responder.Send(Status::Continue);
                    _stage = Stage::Write;
                }
            } else {
                _stage = Stage::Read;
            }

        } break;

        case Control::RejectContent: {
            std::string value;

            if (_request.GetField("Expect", &value)) {
                if (value == "100-continue") {
                    _responder = Responder(_stream);
                    _responder.Send(Status::ExpectationFailed);
                    _stage = Stage::Write;
                }
            } else {
                Close();
            }

        } break;
    }
}

void Channel::OnWrite() {
    auto maxWrite = std::min(_throttlers.Write.Dedicated.GetCurrentQuota(),
                             _throttlers.Write.Master->GetCurrentQuota());

    if (maxWrite == 0) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
        _stage = Stage::WriteTimeout;
        _timeout = _throttlers.Write.Dedicated.GetFillTime();
        return;
    }

    bool doneFlushing = FlushData(maxWrite);

    if (!doneFlushing)
        return;

    // Okay, all data was flushed

    if (_respondedWithError) {
        Log::Default()->Verbose("Channel {} encountered an error and sent an error response", _id);
        Close();
    } else if (_fetchingContent) {
        // This means that we just sent a response requesting
        // the client to send more data to the channel.
        _stage = Stage::Read;
    } else if (_responder.GetKeepAlive()) {
        Log::Default()->Verbose("Channel {} sent response and keeps alive", _id);
        _request = Request(_stream);
        _fetchingContent = false;
        _stage = Stage::Read;
    } else {
        Log::Default()->Verbose("Channel {} sent response and closing", _id);
        Close();
    }
}

bool Channel::FlushData(std::size_t maxWrite) {
    bool done;
    std::size_t bytesFlushed;

    std::tie(done, bytesFlushed) = _responder.Flush(maxWrite);

    _throttlers.Write.Dedicated.Consume(bytesFlushed);
    _throttlers.Write.Master->Consume(bytesFlushed);

    if (!done) {
        if (bytesFlushed < maxWrite) {
            Log::Default()->Verbose("Channel {} socket buffer full. Waiting for writability.", _id);
            _stage = Stage::WaitWritable;
        } else {
            Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
            _stage = Stage::WriteTimeout;
            _timeout = _throttlers.Write.Dedicated.GetFillTime();
        }
    }

    return done;
}

void Channel::Close() {
    if (_stage == Stage::Closed)
        return;

    Log::Default()->Verbose("Channel {} closed", _id);

    _timeout = std::chrono::steady_clock::now();
    _request = Request();
    _responder = Responder();
    _stream.reset();
    _stage = Stage::Closed;
}

const std::shared_ptr<FileStream>& Channel::GetStream() const {
    return _stream;
}

int Channel::GetId() const {
    return _id;
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

} // namespace Http
} // namespace Yam
