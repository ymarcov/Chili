#include "AbstractChannel.h"
#include "ExitTrap.h"
#include "Log.h"

#include <atomic>

namespace Yam {
namespace Http {

std::atomic<std::uint64_t> nextChannelId{0};

AbstractChannel::AbstractChannel(std::shared_ptr<FileStream> stream) :
    _id(++nextChannelId),
    _stream(std::move(stream)),
    _request(_stream),
    _response(_stream),
    _timeout(std::chrono::steady_clock::now()),
    _stage(Stage::WaitReadable) {
    Log::Default()->Verbose("Channel {} created", _id);
}

AbstractChannel::~AbstractChannel() {}

void AbstractChannel::Advance() {
    if (_requestClose) {
        Close();
        return;
    }

    try {
        switch (_stage) {
            case Stage::ReadTimeout:
            case Stage::Read:
                OnRead();
                break;

            case Stage::WriteTimeout:
            case Stage::Write:
                OnWrite();
                break;

            default:
                throw std::logic_error("Advance() called in non-ready stage");
        }
    } catch (const std::exception& e) {
        Log::Default()->Debug("Channel {} error: {}", _id, e.what());
        Close();
    }
}

AbstractChannel::Stage AbstractChannel::GetStage() const {
    return _stage;
}

void AbstractChannel::SetStage(Stage s) {
    if (_stage == Stage::Closed)
        return;

    _stage = s;
}

std::chrono::time_point<std::chrono::steady_clock> AbstractChannel::GetRequestedTimeout() const {
    return _timeout;
}

bool AbstractChannel::IsReady() const {
    if (std::chrono::steady_clock::now() < _timeout.load())
        return false;

    auto stage = _stage.load();

    return (stage != Stage::WaitReadable) &&
            (stage != Stage::WaitWritable) &&
            (stage != Stage::Closed);
}

bool AbstractChannel::IsWaitingForClient() const {
    auto stage = _stage.load();

    return (stage == Stage::WaitReadable) ||
            (stage == Stage::WaitWritable);
}

void AbstractChannel::OnRead() {
    auto maxRead = std::min(_throttlers.Read.Dedicated.GetCurrentQuota(),
                            _throttlers.Read.Master->GetCurrentQuota());

    auto minCapacity = std::min(_throttlers.Read.Dedicated.GetCapacity(),
                                _throttlers.Read.Master->GetCapacity());

    if (maxRead < minCapacity) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
        _stage = Stage::ReadTimeout;
        _timeout = std::max(_throttlers.Read.Dedicated.GetFillTime(minCapacity),
                            _throttlers.Read.Master->GetFillTime(minCapacity));
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
        _response = Response(_stream);

        if (!_request.KeepAlive())
            _response.SetExplicitKeepAlive(false);

        OnProcess();
    }
}

bool AbstractChannel::FetchData(std::pair<bool, std::size_t>(Request::*func)(std::size_t), std::size_t maxRead) {
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
            // if we'd still be limited by the master throttler,
            // it'll work itself out on the next iteration anyway.
            _timeout = _throttlers.Read.Dedicated.GetFillTime();
        }
    }

    return done;
}

void AbstractChannel::LogNewRequest() {
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

void AbstractChannel::OnProcess() {
    _stage = Stage::Process;

    try {
        if (_autoFetchContent && _request.HasContent() && !_request.IsContentAvailable()) {
            if (_request.HasField("Transfer-Encoding"))
                if (_request.GetField("Transfer-Encoding").find("chunked") != std::string::npos)
                    return SendInternalError(); // FIXME support chunked requests

            HandleControlDirective(Control::FetchContent);
        } else {
            auto directive = Process();
            HandleControlDirective(directive);
        }
    } catch (...) {
        SendInternalError();
        return;
    }
}

void AbstractChannel::SendInternalError() {
    Log::Default()->Error("Channel {} processor error ignored! Please handle internally.", _id);
    _forceClose = true;
    _response = Response(_stream);
    _response.SetExplicitKeepAlive(false);
    _response.Send(Status::InternalServerError);
    _stage = Stage::Write;
}

void AbstractChannel::HandleControlDirective(Control directive) {
    switch (directive) {
        case Control::SendResponse:
            _stage = Stage::Write;
            break;

        case Control::FetchContent: {
            _fetchingContent = true;
            std::string value;

            if (_request.GetField("Expect", &value)) {
                if (value == "100-continue") {
                    _response = Response(_stream);
                    _response.Send(Status::Continue);
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
                    _response = Response(_stream);
                    _response.Send(Status::ExpectationFailed);
                    _stage = Stage::Write;
                }
            } else {
                Close();
            }

        } break;
    }
}

void AbstractChannel::OnWrite() {
    auto maxWrite = std::min(_throttlers.Write.Dedicated.GetCurrentQuota(),
                             _throttlers.Write.Master->GetCurrentQuota());

    auto minCapacity = std::min(_throttlers.Write.Dedicated.GetCapacity(),
                                _throttlers.Write.Master->GetCapacity());

    if (maxWrite < minCapacity) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
        _stage = Stage::WriteTimeout;
        _timeout = std::max(_throttlers.Write.Dedicated.GetFillTime(minCapacity),
                            _throttlers.Write.Master->GetFillTime(minCapacity));
        return;
    }

    bool doneFlushing = FlushData(maxWrite);

    if (!doneFlushing)
        return;

    // Okay, all data was flushed

    if (_forceClose) {
        Close();
    } else if (_fetchingContent) {
        // This means that we just sent a response requesting
        // the client to send more data to the channel.
        _stage = Stage::Read;
    } else if (_response.GetKeepAlive()) {
        Log::Default()->Verbose("Channel {} sent response and keeps alive", _id);
        _request = Request(_stream);
        _fetchingContent = false;
        _stage = Stage::Read;
    } else {
        Log::Default()->Verbose("Channel {} sent final response", _id);
        Close();
    }
}

bool AbstractChannel::FlushData(std::size_t maxWrite) {
    std::size_t bytesFlushed = 0;

    auto alwaysConsume = CreateExitTrap([&] {
        _throttlers.Write.Dedicated.Consume(bytesFlushed);
        _throttlers.Write.Master->Consume(bytesFlushed);
    });

    bool done = _response.Flush(maxWrite, bytesFlushed);

    if (!done) {
        if (bytesFlushed < maxWrite) {
            if (bytesFlushed < _response.GetBufferSize()) {
                Log::Default()->Verbose("Channel {} socket buffer full. Waiting for writability.", _id);
                _stage = Stage::WaitWritable;
            }
        } else {
            Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
            _stage = Stage::WriteTimeout;
            // if we'd still be limited by the master throttler,
            // it'll work itself out on the next iteration anyway.
            _timeout = _throttlers.Write.Dedicated.GetFillTime();
        }
    }

    return done;
}

void AbstractChannel::Close() {
    if (_stage == Stage::Closed)
        return;

    Log::Default()->Verbose("Channel {} closed", _id);

    _timeout = std::chrono::steady_clock::now();
    _request = Request();
    _response = Response();
    _stream.reset();
    _stage = Stage::Closed;
}

const std::shared_ptr<FileStream>& AbstractChannel::GetStream() const {
    return _stream;
}

int AbstractChannel::GetId() const {
    return _id;
}

void AbstractChannel::RequestClose() {
    _requestClose = true;
}

bool AbstractChannel::IsCloseRequested() const {
    return _requestClose;
}

} // namespace Http
} // namespace Yam
