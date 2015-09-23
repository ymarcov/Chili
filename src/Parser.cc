#include "Parser.h"

namespace Yam {
namespace Http {

Parser::Parser(const char* buf, std::size_t bufSize) :
    _lexer{buf, bufSize} {}

void Parser::Parse() {
    ParseRequestLine();
    while (!EndOfHeader())
        ParseNextFieldLine();
    SkipToBody();
}

Parser::Field Parser::GetBody() const {
    auto r = _lexer.GetRemaining();
    return {r.first, r.second};
}

Parser::Field Parser::GetField(const std::string& name) const {
    auto i = _fields.find(name);

    if (i != end(_fields))
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
    auto field = _fields.find("Cookie");

    if (field == end(_fields))
        return;

    Lexer lexer{field->second.Data, field->second.Size};

    lexer.SetDelimeters({"=", ";", ",", " ", "\t"});

    while (!lexer.EndOfStream()) {

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
    return _fields.at(RequestField::Method);
}

Parser::Field Parser::GetUri() const {
    return _fields.at(RequestField::Uri);
}

Parser::Field Parser::GetProtocolVersion() const {
    return _fields.at(RequestField::Version);
}

void Parser::ParseRequestLine() {
    _lexer.SetDelimeters({" ", "\t", "\r", "\n"});

    auto word = _lexer.Next();
    _fields[RequestField::Method] = {word.first, word.second};
    word = _lexer.Next();
    _fields[RequestField::Uri] = {word.first, word.second};
    word = _lexer.Next();
    _fields[RequestField::Version] = {word.first, word.second};
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

    _fields[{key.first, key.second}] = {value.first, value.second};
}

} // namespace Http
} // namespace Yam

