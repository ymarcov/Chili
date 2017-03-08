#include "Channel.h"

namespace Yam {
namespace Http {

namespace {

std::unique_ptr<char[]> AllocateRequestBuffer() {
    return std::unique_ptr<char[]>(new Request::Buffer);
}

} // unnamed namespace

Channel::Channel(std::shared_ptr<FileStream> stream, Throttlers throttlers) :
    _stream(std::move(stream)),
    _throttlers(std::move(throttlers)),
    _requestBuffer(AllocateRequestBuffer()),
    _request(_requestBuffer, _stream),
    _responder(_stream),
    _stage(Stage::WaitReadable) {
}

void Channel::PerformStage() {
    std::lock_guard<std::mutex> lock(_mutex);

    try {
        switch (_stage) {
            case Stage::Read:
                OnRead();
                break;

            case Stage::Process:
                OnProcess();
                break;

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
    std::lock_guard<std::mutex> lock(_mutex);
    return _stage;
}

void Channel::SetStage(Stage s) {
    std::lock_guard<std::mutex> lock(_mutex);
    _stage = s;
}

std::chrono::time_point<std::chrono::steady_clock> Channel::GetTimeout() const {
    return _timeout;
}

bool Channel::IsReady() const {
    std::lock_guard<std::mutex> lock(_mutex);

    if (std::chrono::steady_clock::now() < _timeout)
        return false;

    return (_stage != Stage::WaitReadable) &&
            (_stage != Stage::WaitWritable) &&
            (_stage != Stage::Closed);
}

void Channel::OnRead() {
    auto maxRead = std::min(_throttlers.Read.Dedicated.GetCurrentQuota(),
                            _throttlers.Read.Master->GetCurrentQuota());

    if (maxRead == 0) {
        _stage = Stage::WaitReadable;
        _timeout = _throttlers.Read.Dedicated.GetFillTimePoint();
        return;
    }

    if (_fetchBody) {
        auto result = _request.ConsumeBody(_body.data() + _bodyPosition,
                                           std::min(maxRead, _body.size() - _bodyPosition));

        _throttlers.Read.Dedicated.Consume(result.second);
        _throttlers.Read.Master->Consume(result.second);

        if (!result.first) {
            _bodyPosition += result.second;
            _stage = Stage::WaitReadable;
            return;
        }
    } else {
        auto result = _request.ConsumeHeader(maxRead);

        _throttlers.Read.Dedicated.Consume(result.second);
        _throttlers.Read.Master->Consume(result.second);

        if (!result.first) {
            _stage = Stage::WaitReadable;
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
        result = Process(_request, _responder);
    } catch (...) {
        // TODO: log?
        _error = true;
        _responder = Responder(_stream);
        _responder.Send(Status::InternalServerError);
        return;
    }

    if (result == Control::FetchBody) {
        _fetchBody = true;
        _bodyPosition = 0;
        _body = std::vector<char>(_request.GetContentLength());
    }
}

void Channel::OnWrite() {
    auto maxWrite = std::min(_throttlers.Write.Dedicated.GetCurrentQuota(),
                             _throttlers.Write.Master->GetCurrentQuota());

    if (maxWrite == 0) {
        _stage = Stage::WaitWritable;
        _timeout = _throttlers.Write.Dedicated.GetFillTimePoint();
        return;
    }

    auto result = _responder.Flush(maxWrite);

    _throttlers.Write.Dedicated.Consume(result.second);
    _throttlers.Write.Master->Consume(result.second);

    if (!result.first) {
        _stage = Stage::WaitWritable;
    } else {
        if (_error || !_responder.GetKeepAlive()) {
            Close();
        } else {
            _request = Request(_requestBuffer, _stream);
            _fetchBody = false;
            _stage = Stage::WaitReadable;
        }
    }
}

void Channel::Close() {
    _stage = Stage::Closed;
    _timeout = std::chrono::steady_clock::now();
    _request = Request();
    _responder = Responder();
    _stream.reset();
}

} // namespace Http
} // namespace Yam
