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
        // Got an async close request, most likely because
        // the underlying socket has been closed.
        Close();
        return;
    }

    try {
        switch (_stage) {
            case Stage::ReadTimeout: // returned from timeout
            case Stage::Read:
                OnRead();
                break;

            case Stage::WriteTimeout: // returned from timeout
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
    // Set timeout (probably due to throttling) has not yet expired.
    if (std::chrono::steady_clock::now() < _timeout.load())
        return false;

    // If we're closed, we don't have anything
    // else to do. We just need to wait to be
    // garbaged-collected.
    if (_stage == Stage::Closed)
        return true;

    return !IsWaitingForClient();
}

bool AbstractChannel::IsWaitingForClient() const {
    auto stage = _stage.load();

    return (stage == Stage::WaitReadable) ||
            (stage == Stage::WaitWritable);
}

void AbstractChannel::OnRead() {
    auto throttlingInfo = GetThrottlingInfo(_throttlers.Read);

    if (!throttlingInfo.full) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
        _stage = Stage::ReadTimeout;
        _timeout = throttlingInfo.fillTime;
        return;
    }

    bool doneReading;

    if (_fetchingContent) {
        // We're fetching more content from the request
        if ((doneReading = FetchData(&Request::ConsumeContent, throttlingInfo.currentQuota)))
            _fetchingContent = false;
    } else {
        // We're fetching the request's header
        if ((doneReading = FetchData(&Request::ConsumeHeader, throttlingInfo.currentQuota)))
            LogNewRequest();
    }

    if (doneReading) {
        // All done reading, prepare a new response

        _response = Response(_stream);

        // If the request does not want us to keep alive,
        // we will explicitly agree to. Otherwise, HTTP/1.1
        // requires us to indeed keep alive by default.
        if (!_request.KeepAlive())
            _response.SetExplicitKeepAlive(false);

        // Ready to process
        OnProcess();
    }
}

bool AbstractChannel::FetchData(bool(Request::*func)(std::size_t, std::size_t&), std::size_t maxRead) {
    std::size_t bytesFetched = 0;

    // Make sure throttling remains consistent even
    // if we encountered an error during any stage.
    auto alwaysConsume = CreateExitTrap([&] {
        _throttlers.Read.Dedicated.Consume(bytesFetched);
        _throttlers.Read.Master->Consume(bytesFetched);
    });

    // Fetch more data.
    // The logic is the same here for fetching both the
    // header and the content, and so we simply use a
    // member function pointer to avoid duplication.
    bool done = (_request.*func)(maxRead, bytesFetched);

    if (!done) {
        // More data needs to be fetched

        if (bytesFetched < maxRead) {
            // If we're *not* done *and* we haven't even read as much
            // as the throttler allowed us to, we can infer that the
            // socket needs to send more data for us to read.
            Log::Default()->Verbose("Channel {} socket buffer empty. Waiting for readability.", _id);
            _stage = Stage::WaitReadable;
        } else {
            // If we're *not* done and we *have* read as much as
            // the throttler allowed us to read, we can infer that
            // the throttler has emptied out, and so we need to
            // wait for it to become full again.
            Log::Default()->Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
            _stage = Stage::ReadTimeout;
            _timeout = GetThrottlingInfo(_throttlers.Read).fillTime;
        }
    }

    return done;
}

void AbstractChannel::OnProcess() {
    _stage = Stage::Process; // Keep it neat (although at the time of writing this isn't used)

    try {
        if (_autoFetchContent && _request.HasContent() && !_request.IsContentAvailable()) {
            // Channel was configured so that we'll automatically get all of
            // the content for it before asking it to process anything.

            { // FIXME: For now we don't support chunked requests
                if (_request.HasField("Transfer-Encoding"))
                    if (_request.GetField("Transfer-Encoding").find("chunked") != std::string::npos)
                        return SendInternalError();
            }

            // So let's get the content
            HandleControlDirective(Control::FetchContent);
        } else {
            // Call derived processing implementation
            auto directive = Process();
            // Do what it says
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
            // Simple enough, just start writing the response
            _stage = Stage::Write;
            break;

        case Control::FetchContent: {
            // This is a flag in our request-reading
            // state-machine that would make it
            // go and fetch more content instead
            // of thinking it's reading a new
            // request header.
            _fetchingContent = true;

            // HTTP has this thing where before clients
            // would keep sending a large request body,
            // they would normally want to server to
            // formally agree to it by issuing a "you
            // may continue" intermediate response.
            std::string value;

            if (_request.GetField("Expect", &value)) {
                // Ok, look, it does want us to send
                // it our confirmation. Let's do it.
                if (value == "100-continue") {
                    _response = Response(_stream);
                    _response.Send(Status::Continue);
                    _stage = Stage::Write;
                }
            } else {
                // It doesn't need our confirmation!
                // Ok, just go straight to reading.
                _stage = Stage::Read;
            }

        } break;

        case Control::RejectContent: {
            std::string value;

            if (_request.GetField("Expect", &value)) {
                // Tough luck client, your request
                // body has been rejected.
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
    auto throttlingInfo = GetThrottlingInfo(_throttlers.Write);

    if (!throttlingInfo.full) {
        Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
        _stage = Stage::WriteTimeout;
        _timeout = throttlingInfo.fillTime;
        return;
    }

    bool doneFlushing = FlushData(throttlingInfo.currentQuota);

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
        _fetchingContent = false; // Will be reading a new request header
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
        // More data needs to be flushed

        if (bytesFlushed < maxWrite) {
            // If we're *not* done *and* we haven't even flushed as much
            // as the throttler allowed us to, we can infer that the
            // socket needs to release more of its buffered data.
            if (bytesFlushed < _response.GetBufferSize()) {
                Log::Default()->Verbose("Channel {} socket buffer full. Waiting for writability.", _id);
                _stage = Stage::WaitWritable;
            }
        } else {
            // If we're *not* done and we *have* flushed as much as
            // the throttler allowed us to flush, we can infer that
            // the throttler has emptied out, and so we need to
            // wait for it to become full again.
            Log::Default()->Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
            _stage = Stage::WriteTimeout;
            _timeout = GetThrottlingInfo(_throttlers.Write).fillTime;
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

AbstractChannel::ThrottlingInfo AbstractChannel::GetThrottlingInfo(const Throttlers::Group& group) const {
    ThrottlingInfo info;

    info.currentQuota = std::min(group.Dedicated.GetCurrentQuota(),
                                 group.Master->GetCurrentQuota());

    info.capacity = std::min(group.Dedicated.GetCapacity(),
                             group.Master->GetCapacity());

    info.full = info.currentQuota == info.capacity;

    if (!info.full)
        info.fillTime = std::max(_throttlers.Read.Dedicated.GetFillTime(info.capacity),
                                 _throttlers.Read.Master->GetFillTime(info.capacity));
    else
        info.fillTime = Throttler::Clock::now();

    return info;
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

} // namespace Http
} // namespace Yam
