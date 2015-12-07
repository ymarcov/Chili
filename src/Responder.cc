#include "Responder.h"

namespace Yam {
namespace Http {

static std::string HttpVersion = "HTTP/1.1";

Responder::Responder(std::shared_ptr<OutputStream> stream) :
    _stream(std::move(stream)) {}

void Responder::Send(Status status) {
    _stream->Write(HttpVersion.data(), HttpVersion.size());

    switch (status) {
        case Status::Continue:
            _stream->Write(" 100 Continue", 13);
            break;
        default:
            throw std::logic_error("Unsupported status response");
    }

    _stream->Write("\r\n", 2);
    _stream->Write("\r\n", 2);
}

} // namespace Http
} // namespace Yam

