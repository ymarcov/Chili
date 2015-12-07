#pragma once

#include "OutputStream.h"
#include "Protocol.h"

#include <memory>

namespace Yam {
namespace Http {

class Responder {
public:
    Responder(std::shared_ptr<OutputStream>);

    void Send(Status);

private:
    std::shared_ptr<OutputStream> _stream;
};

} // namespace Http
} // namespace Yam

