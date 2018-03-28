#include "Channel.h"

namespace Chili {

void Channel::SetAutoFetchContent(bool b) {
    _autoFetchContent = b;
}

Request& Channel::GetRequest() {
    return _request;
}

Response& Channel::GetResponse() {
    return _response;
}

Channel::Control Channel::FetchContent() {
    return Control::FetchContent;
}

Channel::Control Channel::RejectContent() {
    return Control::RejectContent;
}

Channel::Control Channel::SendResponse(std::shared_ptr<CachedResponse> cr) {
    _response.SendCached(std::move(cr));
    return Control::SendResponse;
}

Channel::Control Channel::SendResponse(Status status) {
    _response.Send(status);
    return Control::SendResponse;
}

Channel::Control Channel::SendFinalResponse(Status status) {
    _response.SetExplicitKeepAlive(false);
    _response.Send(status);
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

Channel::Control Channel::Process() {
    return Process(GetRequest(), GetResponse());
}

} // namespace Chili

