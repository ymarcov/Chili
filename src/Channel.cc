#include "Channel.h"

namespace Yam {
namespace Http {

void Channel::SetAutoFetchContent(bool b) {
    _autoFetchContent = b;
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

Channel::Control Channel::SendResponse(std::shared_ptr<CachedResponse> cr) {
    _responder.SendCached(std::move(cr));
    return Control::SendResponse;
}

Channel::Control Channel::SendResponse(Status status) {
    _responder.Send(status);
    return Control::SendResponse;
}

Channel::Control Channel::SendFinalResponse(Status status) {
    _responder.SetExplicitKeepAlive(false);
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

