#pragma once

namespace Yam {
namespace Http {
namespace Protocol {

enum class Method {
    Options,
    Get,
    Head,
    Post,
    Put,
    Delete,
    Trace,
    Connect
};

enum class Version {
    Http10,
    Http11
};

} // namespace Protocol
} // namespace Http
} // namespace Yam

