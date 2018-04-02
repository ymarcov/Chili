#include "Channel.h"
#include "ExitTrap.h"
#include "Log.h"

#include <atomic>
#include <fmt/format.h>

namespace Chili {

std::atomic<std::uint64_t> nextChannelId{1};

ChannelEvent::ChannelEvent(const char* source, std::uint64_t channelId)
    : ChannelId(channelId)
    , _source(source) {}

std::string ChannelEvent::GetSource() const {
    return _source;
}

std::string ChannelEvent::GetSummary() const {
    return fmt::format("Event on channel {}", ChannelId);
}

void ChannelEvent::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelActivated::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelReadable::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelWritable::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelCompleted::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

ChannelTimeout::ChannelTimeout(const char* source,
                               std::uint64_t channelId,
                               Clock::TimePoint throttledTime,
                               Clock::TimePoint readyTime) :
    ChannelEvent(source, channelId)
    , ThrottledTime(throttledTime)
    , ReadyTime(readyTime) {}

void ChannelReadTimeout::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelWriteTimeout::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelWaitReadable::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelWaitWritable::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelReading::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelWriting::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelWritten::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

void ChannelClosed::Accept(ProfileEventReader& reader) const {
    reader.Read(*this);
}

Channel::Channel(std::shared_ptr<FileStream> stream) :
    _id(nextChannelId++),
    _stream(std::move(stream)),
    _request(_stream),
    _response(_stream),
    _timeout(Clock::GetCurrentTime()),
    _stage(Stage::WaitReadable) {
    Log::Verbose("Channel {} created", _id);
}

Channel::~Channel() {}

/**
 * Public API
 */

void Channel::SetAutoFetchContent(bool b) {
    _autoFetchContent = b;
}

Request& Channel::GetRequest() {
    return _request;
}

Response& Channel::GetResponse() {
    return _response;
}

void Channel::FetchContent(std::function<void()> callback) {
    _fetchContentCallback = std::move(callback);
    _controlDirective = Control::FetchContent;
}

void Channel::RejectContent() {
    _controlDirective = Control::RejectContent;
}

void Channel::SendResponse(std::shared_ptr<CachedResponse> cr) {
    _response.SendCached(std::move(cr));
    _controlDirective = Control::SendResponse;
}

void Channel::SendResponse(Status status) {
    _response.Send(status);
    _controlDirective = Control::SendResponse;
}

void Channel::SendFinalResponse(Status status) {
    _response.SetExplicitKeepAlive(false);
    _response.Send(status);
    _controlDirective = Control::SendResponse;
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

/**
 * Internal API
 */

void Channel::Advance() {
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
        Log::Debug("Channel {} error: {}", _id, e.what());
        Close();
    }
}

Channel::Stage Channel::GetTentativeStage() const {
    return _stage.load(std::memory_order_relaxed);
}

Channel::Stage Channel::GetDefiniteStage() const {
    return _stage.load();
}

void Channel::SetStage(Stage s) {
    if (_stage == Stage::Closed)
        return;

    _stage = s;
}

Clock::TimePoint Channel::GetRequestedTimeout() const {
    return _timeout;
}

void Channel::SetRequestedTimeout(Clock::TimePoint t) {
    _timeout = t;
}

bool Channel::IsReady() const {
    // Set timeout (probably due to throttling) has not yet expired.
    if (Clock::GetCurrentTime() < GetRequestedTimeout())
        return false;

    // If we're closed, we don't have anything
    // else to do. We just need to wait to be
    // garbaged-collected.
    if (_stage == Stage::Closed)
        return true;

    return !IsWaitingForClient();
}

bool Channel::IsWaitingForClient() const {
    auto stage = _stage.load();

    return (stage == Stage::WaitReadable) ||
            (stage == Stage::WaitWritable);
}

void Channel::OnRead() {
    auto throttlingInfo = GetThrottlingInfo(_throttlers.Read);

    if (!throttlingInfo.full) {
        Log::Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
        RecordReadTimeoutEvent(throttlingInfo.fillTime);
        _stage = Stage::ReadTimeout;
        SetRequestedTimeout(throttlingInfo.fillTime);
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

bool Channel::FetchData(bool(Request::*func)(std::size_t, std::size_t&), std::size_t maxRead) {
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
            Log::Verbose("Channel {} socket buffer empty. Waiting for readability.", _id);
            RecordProfileEvent<ChannelWaitReadable>();
            _stage = Stage::WaitReadable;
        } else {
            // If we're *not* done and we *have* read as much as
            // the throttler allowed us to read, we can infer that
            // the throttler has emptied out, and so we need to
            // wait for it to become full again.
            Log::Verbose("Channel {} throttled. Waiting for read quota to fill.", _id);
            auto fillTime = GetThrottlingInfo(_throttlers.Read).fillTime;
            RecordReadTimeoutEvent(fillTime);
            _stage = Stage::ReadTimeout;
            SetRequestedTimeout(fillTime);
        }
    }

    return done;
}

void Channel::OnProcess() {
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
            _controlDirective = Control::FetchContent;
        } else {
            if (_fetchContentCallback) {
                // Call callback set by previous call to Process()
                _fetchContentCallback();
                _fetchContentCallback = {};
            } else {
                // Call derived processing implementation
                // and let it set _controlDirective
                Process();
            }
        }

        HandleControlDirective();
    } catch (...) {
        SendInternalError();
        return;
    }
}

void Channel::SendInternalError() {
    Log::Error("Channel {} processor error ignored! Please handle internally.", _id);
    _forceClose = true;
    _response = Response(_stream);
    _response.SetExplicitKeepAlive(false);
    _response.Send(Status::InternalServerError);
    RecordProfileEvent<ChannelWriting>();
    _stage = Stage::Write;
}

void Channel::HandleControlDirective() {
    switch (_controlDirective) {
        case Control::SendResponse:
            // Simple enough, just start writing the response
            RecordProfileEvent<ChannelWriting>();
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
                    RecordProfileEvent<ChannelWriting>();
                    _stage = Stage::Write;
                }
            } else {
                // It doesn't need our confirmation!
                // Ok, just go straight to reading.
                RecordProfileEvent<ChannelReading>();
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
                    RecordProfileEvent<ChannelWriting>();
                    _stage = Stage::Write;
                }
            } else {
                Close();
            }

        } break;
    }
}

void Channel::OnWrite() {
    auto throttlingInfo = GetThrottlingInfo(_throttlers.Write);

    if (!throttlingInfo.full) {
        Log::Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
        RecordWriteTimeoutEvent(throttlingInfo.fillTime);
        _stage = Stage::WriteTimeout;
        SetRequestedTimeout(throttlingInfo.fillTime);
        return;
    }

    bool doneFlushing = FlushData(throttlingInfo.currentQuota);

    if (!doneFlushing)
        return;

    // Okay, all data was flushed

    RecordProfileEvent<ChannelWritten>();

    if (_forceClose) {
        Close();
    } else if (_fetchingContent) {
        // This means that we just sent a response requesting
        // the client to send more data to the channel.
        RecordProfileEvent<ChannelReading>();
        _stage = Stage::Read;
    } else if (_response.GetKeepAlive()) {
        Log::Verbose("Channel {} sent response and keeps alive", _id);
        _request = Request(_stream);
        _fetchingContent = false; // Will be reading a new request header
        RecordProfileEvent<ChannelReading>();
        _stage = Stage::Read;
    } else {
        Log::Verbose("Channel {} sent final response", _id);
        Close();
    }
}

bool Channel::FlushData(std::size_t maxWrite) {
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
                Log::Verbose("Channel {} socket buffer full. Waiting for writability.", _id);
                RecordProfileEvent<ChannelWaitWritable>();
                _stage = Stage::WaitWritable;
            }
        } else {
            // If we're *not* done and we *have* flushed as much as
            // the throttler allowed us to flush, we can infer that
            // the throttler has emptied out, and so we need to
            // wait for it to become full again.
            Log::Verbose("Channel {} throttled. Waiting for write quota to fill.", _id);
            auto fillTime = GetThrottlingInfo(_throttlers.Write).fillTime;
            RecordWriteTimeoutEvent(fillTime);
            _stage = Stage::WriteTimeout;
            SetRequestedTimeout(fillTime);
        }
    }

    return done;
}

void Channel::Close() {
    if (_stage == Stage::Closed)
        return;

    Log::Verbose("Channel {} closed", _id);

    SetRequestedTimeout(Clock::GetCurrentTime());
    _request = Request();
    _response = Response();
    _stream.reset();
    RecordProfileEvent<ChannelClosed>();
    _stage = Stage::Closed;
}

const std::shared_ptr<FileStream>& Channel::GetStream() const {
    return _stream;
}

int Channel::GetId() const {
    return _id;
}

Channel::ThrottlingInfo Channel::GetThrottlingInfo(const Throttlers::Group& group) const {
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
        info.fillTime = Clock::GetCurrentTime();

    return info;
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

        case Method::Put:
            method = "PUT";
            break;

        case Method::Delete:
            method = "DELETE";
            break;

        default:
            Log::Info("Unsupported method for {}! Dropping request on channel {}.", _request.GetUri(), _id);
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

    Log::Info("Channel {} Received \"{} {} {}\"", _id, method, _request.GetUri(), version);
}

} // namespace Chili
