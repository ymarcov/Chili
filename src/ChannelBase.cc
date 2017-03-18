#include "ChannelBase.h"

namespace Yam {
namespace Http {

void ChannelBase::SetAutoFetchContent(bool b) {
    _autoFetchContent = b;
}

Request& ChannelBase::GetRequest() {
    return _request;
}

Responder& ChannelBase::GetResponder() {
    return _responder;
}

ChannelBase::Control ChannelBase::FetchContent() {
    return Control::FetchContent;
}

ChannelBase::Control ChannelBase::RejectContent() {
    return Control::RejectContent;
}

ChannelBase::Control ChannelBase::SendResponse(std::shared_ptr<CachedResponse> cr) {
    _responder.SendCached(std::move(cr));
    return Control::SendResponse;
}

ChannelBase::Control ChannelBase::SendResponse(Status status) {
    _responder.Send(status);
    return Control::SendResponse;
}

ChannelBase::Control ChannelBase::SendFinalResponse(Status status) {
    _responder.SetExplicitKeepAlive(false);
    _responder.Send(status);
    return Control::SendResponse;
}

bool ChannelBase::IsReadThrottled() const {
    return _throttlers.Read.Dedicated.IsEnabled();
}

bool ChannelBase::IsWriteThrottled() const {
    return _throttlers.Write.Dedicated.IsEnabled();
}

void ChannelBase::ThrottleRead(Throttler t) {
    _throttlers.Read.Dedicated = std::move(t);
}

void ChannelBase::ThrottleWrite(Throttler t) {
    _throttlers.Write.Dedicated = std::move(t);
}

} // namespace Http
} // namespace Yam

