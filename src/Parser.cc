#include "Parser.h"

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

void Parser::SaveFieldLocation(FieldType type, Field field) {
    _requestLineFields[static_cast<int>(type)] = field;
}

Parser::Field Parser::GetField(FieldType type) const {
    auto i = _requestLineFields.find(static_cast<int>(type));

    if (i != end(_requestLineFields))
        return i->second;
    else
        throw Error("Field does not exist");
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

Parser::Field Parser::GetMethod() const {
    return GetField(FieldType::Method);
}

Parser::Field Parser::GetUri() const {
    return GetField(FieldType::Uri);
}

Parser::Field Parser::GetProtocolVersion() const {
    return GetField(FieldType::ProtocolVersion);
}

void Parser::ParseRequestLine() {
    SaveFieldLocation(FieldType::Method, ParseUntil(' '));
    SaveFieldLocation(FieldType::Uri, ParseUntil(' '));
    SaveFieldLocation(FieldType::ProtocolVersion, ParseRestOfLine());
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

    // tolerantly allow for an _optional_ space after colon
    if (_remainingChars && *_positionedBuffer == ' ') {
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

