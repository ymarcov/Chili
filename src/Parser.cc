#include "Parser.h"

#include <string.h>

namespace Yam {
namespace Http {

Parser::Lexer::Lexer(const char* stream, std::size_t length) :
    _stream{stream},
    _initialLength{length},
    _length{length} {}

void Parser::Lexer::SetDelimeters(std::initializer_list<const char*> delimeters) {
    if (_delimeters.size() < delimeters.size())
        throw std::logic_error("Too many delimeters");

    _delimeterCount = 0;
    _usableDelimeterData = _delimeterData.size();

    for (const char* d : delimeters)
        AddDelimeter(d);
}

std::size_t Parser::Lexer::GetConsumption() const {
    return _initialLength - _length;
}

bool Parser::Lexer::EndOfStream() const {
    return !_length;
}

std::size_t Parser::Lexer::Compress() {
    std::size_t stride = ConsumeDelimeters(_stream, 0);
    _stream += stride;
    _length -= stride;
    return stride;
}

std::pair<const char*, std::size_t> Parser::Lexer::Next(bool compress) {
    const char* startingPoint = _stream;
    const char* cursor = startingPoint;
    std::size_t wordLength = 0;

    while (_length > wordLength) {
        if (std::size_t delimLength = DelimeterAt(cursor, wordLength)) {
            std::size_t stride = wordLength;

            if (compress)
                stride += ConsumeDelimeters(cursor, wordLength);
            else
                stride += delimLength;

            _stream += stride;
            _length -= stride;

            return {startingPoint, wordLength};
        } else {
            ++cursor;
            ++wordLength;
        }
    }

    // reached end of stream

    _stream += wordLength;
    _length -= wordLength;

    return {startingPoint, wordLength};
}

std::size_t Parser::Lexer::ConsumeDelimeters(const char* cursor, std::size_t consumed) {
    std::size_t stride = 0;

    while (_length > consumed + stride)
        if (auto delimLength = DelimeterAt(cursor, consumed + stride)) {
            cursor += delimLength;
            stride += delimLength;
        } else {
            break;
        }

    return stride;
}

void Parser::Lexer::AddDelimeter(const char* delimeter) {
    std::size_t length = ::strnlen(delimeter, _usableDelimeterData + 1);

    if (length > _usableDelimeterData)
        throw std::logic_error("Delimeter too large to fit buffer");

    std::size_t dataOffset = _delimeterData.size() - _usableDelimeterData;
    char* dataSlot = _delimeterData.data() + dataOffset;
    ::strncpy(dataSlot, delimeter, length);

    _delimeterLengths[_delimeterCount] = length;
    _delimeters[_delimeterCount] = dataSlot;

    _usableDelimeterData -= length;
    ++_delimeterCount;

}

std::size_t Parser::Lexer::DelimeterAt(const char* cursor, std::size_t consumed) {
    for (std::size_t i = 0; i < _delimeterCount; ++i)
        if (_length > consumed + (_delimeterLengths[i] - 1))
            if (!::strncmp(cursor, _delimeters[i], _delimeterLengths[i]))
                return _delimeterLengths[i];
    return 0;
}

/* Parser functions */

Parser::Parser(const char* buf, std::size_t bufSize) :
    _lexer{buf, bufSize},
    _positionedBuffer{buf},
    _remainingChars{bufSize} {}

void Parser::Parse() {
    ParseRequestLine();
    while (!EndOfHeader())
        ParseNextFieldLine();
    SkipToBody();
}

Parser::Field Parser::GetBody() const {
    auto c = _lexer.GetConsumption();
    return {_positionedBuffer + c, _remainingChars - c};
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

    Lexer lexer{field->second.Data, field->second.Size};

    while (!lexer.EndOfStream()) {
        lexer.SetDelimeters({"=", ";", ",", " ", "\t"});

        auto name = lexer.Next();
        auto value = lexer.Next();

        _cookies[{name.first, name.second}] = {value.first, value.second};
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
    _lexer.SetDelimeters({" ", "\t", "\r", "\n"});

    auto word = _lexer.Next();
    _extraFields[RequestField::Method] = {word.first, word.second};
    word = _lexer.Next();
    _extraFields[RequestField::Uri] = {word.first, word.second};
    word = _lexer.Next();
    _extraFields[RequestField::Version] = {word.first, word.second};
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

bool Parser::EndOfHeader() {
    Lexer lexer = _lexer;
    lexer.SetDelimeters({"\r\n", "\r", "\n"});
    return !lexer.Next(false).second;
}

void Parser::SkipToBody() {
    _lexer.SetDelimeters({"\r\n", "\r", "\n"});
    _lexer.Next(false);
}

void Parser::ParseNextFieldLine() {
    _lexer.SetDelimeters({" ", "\t", ":"});
    auto key = _lexer.Next();

    _lexer.SetDelimeters({"\r\n", "\r", "\n"});
    auto value = _lexer.Next(false);

    _extraFields[{key.first, key.second}] = {value.first, value.second};
}

} // namespace Http
} // namespace Yam

