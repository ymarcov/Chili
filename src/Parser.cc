#include "Parser.h"
#include <array>

namespace Yam {
namespace Http {

Parser::Parser(const char* buf, std::size_t bufSize) :
    _positionedBuffer(buf),
    _remainingChars(bufSize) {
}

void Parser::Parse() {
    ParseRequestLine();
    while (!EndOfHeader())
        ParseNextFieldLine();
    SkipToBody();
}

const char* Parser::GetBody() const {
    return _positionedBuffer;
}

Parser::Field Parser::GetField(const std::string& name) const {
    auto i = _extraFields.find(name);

    if (i != end(_extraFields))
        return i->second;
    else
        throw Error("Field does not exist");
}

Parser::Field Parser::GetCookie(const std::string& name) const {
    if (!_cookiesHaveBeenParsed)
        ParseCookies();

    auto i = _cookies.find(name);

    if (i != end(_cookies))
        return i->second;
    else
        throw Error("Cookie does not exist");
}

std::vector<Parser::Field> Parser::GetCookieNames() const {
    if (!_cookiesHaveBeenParsed)
        ParseCookies();

    std::vector<Parser::Field> result;
    result.reserve(_cookies.size());

    for (auto& kv : _cookies)
        result.emplace_back(Field{kv.first.data(), kv.first.size()});

    return result;
}

void Parser::ParseCookies() const {
    auto field = _extraFields.find("Cookie");

    if (field == end(_extraFields))
        return;

    auto ptr = field->second.Data;
    auto remainingChars = field->second.Size;

    Field name = {ptr, 0};
    Field value;
    bool parsingName = true;

    while (remainingChars) {
        if (parsingName) {
            while (remainingChars && *ptr != '=') {
                ++ptr;
                ++name.Size;
                --remainingChars;
            }

            // if we hit '=', skip it and continue
            if (remainingChars) {
                ++ptr;
                --remainingChars;
            }

            value.Data = ptr;
            value.Size = 0;

            parsingName = false;
        } else { // parsing value
            while (remainingChars && *ptr != ';') {
                ++ptr;
                ++value.Size;
                --remainingChars;
            }

            // if we hit ';', skip it and continue.
            if (remainingChars) {
                ++ptr;
                --remainingChars;
            }

            // NOTE: here is where we insert the cookie to the cache
            _cookies[{name.Data, name.Size}] = value;

            // tolerate spaces between cookies
            while (remainingChars && (*ptr == ' ' || *ptr == '\t')) {
                ++ptr;
                --remainingChars;
            }

            name.Data = ptr;
            name.Size = 0;

            parsingName = true; // ???
        }
    }

    _cookiesHaveBeenParsed = true;
}

namespace {
namespace RequestField {

static const std::string Method = "$req_method";
static const std::string Uri = "$req_uri";
static const std::string Version = "$req_version";

} // namespace RequestField
} // unnamed namespace

Parser::Field Parser::GetMethod() const {
    return _extraFields.at(RequestField::Method);
}

Parser::Field Parser::GetUri() const {
    return _extraFields.at(RequestField::Uri);
}

Parser::Field Parser::GetProtocolVersion() const {
    return _extraFields.at(RequestField::Version);
}

void Parser::ParseRequestLine() {
    _extraFields[RequestField::Method] = ParseUntil(' ');
    _extraFields[RequestField::Uri] = ParseUntil(' ');
    _extraFields[RequestField::Version] = ParseRestOfLine();
}

Parser::Field Parser::ParseUntil(char delimeter) {
    Field field;

    field.Data = _positionedBuffer;
    field.Size = 0;

    while (_remainingChars-- && *_positionedBuffer++ != delimeter)
        ++field.Size;

    return field;
}

Parser::Field Parser::ParseRestOfLine() {
    Field f = ParseUntil('\r');
    ++_positionedBuffer;
    --_remainingChars;
    return f;
}

bool Parser::EndOfHeader() const {
    auto buf = _positionedBuffer;
    return (_remainingChars < 2) || (buf[0] == '\r' && buf[1] == '\n');
}

void Parser::ParseNextFieldLine() {
    Field key = ParseUntil(':');

    // tolerate spaces after colon
    while (_remainingChars &&
            (*_positionedBuffer == ' ' || *_positionedBuffer == '\t')) {
        ++_positionedBuffer;
        --_remainingChars;
    }

    _extraFields[{key.Data, key.Size}] = ParseRestOfLine();
}

void Parser::SkipToBody() {
    if (_remainingChars && *_positionedBuffer == '\r') {
        ++_positionedBuffer;
        --_remainingChars;
    }

    if (_remainingChars && *_positionedBuffer == '\n') {
        ++_positionedBuffer;
        --_remainingChars;
    }
}

} // namespace Http
} // namespace Yam

