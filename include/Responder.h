#pragma once

#include "OutputStream.h"
#include "Protocol.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Yam {
namespace Http {

class Responder {
public:
    Responder(std::shared_ptr<OutputStream>);

    void Send(Status);
    void SetField(std::string name, std::string value);
    void SetCookie(std::string name, std::string value);
    void SetBody(std::shared_ptr<std::vector<char>>);

private:
    std::shared_ptr<OutputStream> _stream;
    std::vector<std::pair<std::string, std::string>> _fields;
    std::shared_ptr<std::vector<char>> _body;
};

} // namespace Http
} // namespace Yam

