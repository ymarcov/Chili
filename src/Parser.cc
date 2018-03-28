#include "Parser.h"

namespace Chili {

namespace {
namespace RequestField {

static const char Marker = '$';
static const std::string Method = "$req_method";
static const std::string Uri = "$req_uri";
static const std::string Version = "$req_version";

} // namespace RequestField
} // unnamed namespace

Parser::Error::Error(const char* what) :
    Error{Type::None, what} {}

Parser::Error::Error(Parser::Error::Type t, const char* what) :
    std::runtime_error{what},
    _type{t} {}

Parser Parser::Parse(const char* buf, std::size_t bufSize) {
    Parser p(buf, bufSize);
    p.ParseAll();
    return p;
}

Parser::Parser(const char* buf, std::size_t bufSize) :
    _lexer{buf, bufSize} {}

void Parser::ParseAll() {
    ParseRequestLine();
    while (!EndOfHeader())
        ParseNextFieldLine();
    SkipToBody();
}

const char* Parser::GetBody() const {
    return _lexer.GetRemaining().first;
}

std::size_t Parser::GetHeaderLength() const {
    return _lexer.GetConsumption();
}

Parser::Field Parser::GetField(const std::string_view& name) const {
    auto i = _fields.find(name);

    if (i != end(_fields))
        return i->second;
    else
        throw Error{"Field does not exist"};
}

bool Parser::GetField(const std::string_view& name, Field* value) const {
    auto i = _fields.find(name);
    auto found = i != end(_fields);

    if (found && value)
        *value = i->second;

    return found;
}

std::vector<Parser::Field> Parser::GetFieldNames() const {
    std::vector<Parser::Field> result;
    result.reserve(_fields.size() - 3 /* request line fields */);

    for (auto& kv : _fields)
        if (kv.first[0] != RequestField::Marker)
            result.emplace_back(Field{kv.first.data(), kv.first.size()});

    return result;
}

Parser::Field Parser::GetCookie(const std::string_view& name) const {
    if (!_cookiesHaveBeenParsed)
        ParseCookies();

    auto i = _cookies.find(name);

    if (i != end(_cookies))
        return i->second;
    else
        throw Error{"Cookie does not exist"};
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
        auto name = lexer.Lex();
        auto value = lexer.Lex();

        _cookies[{name.first, name.second}] = {value.first, value.second};
    }

    _cookiesHaveBeenParsed = true;
}

Parser::Field Parser::GetMethod() const {
    return _fields.at(RequestField::Method);
}

Parser::Field Parser::GetUri() const {
    return _fields.at(RequestField::Uri);
}

Parser::Field Parser::GetVersion() const {
    return _fields.at(RequestField::Version);
}

void Parser::ParseRequestLine() {
    _lexer.SetDelimeters({" ", "\t"});

    auto method = _lexer.Lex();
    _fields[RequestField::Method] = {method.first, method.second};

    auto uri = _lexer.Lex();
    _fields[RequestField::Uri] = {uri.first, uri.second};

    _lexer.SetDelimeters({"\r\n", "\r", "\n"});

    auto protocol = _lexer.Lex(false);
    _fields[RequestField::Version] = {protocol.first, protocol.second};
}

bool Parser::EndOfHeader() {
    Lexer lexer = _lexer;
    lexer.SetDelimeters({"\r\n", "\r", "\n"});
    return !lexer.Lex(false).second;
}

void Parser::SkipToBody() {
    _lexer.SetDelimeters({"\r\n", "\r", "\n"});
    auto c = _lexer.GetConsumption();
    _lexer.Lex(false);
    if (c == _lexer.GetConsumption())
        throw Error{Error::Type::Malformed, "Request header did not end in blank line"};
}

void Parser::ParseNextFieldLine() {
    _lexer.SetDelimeters({" ", "\t", ":"});
    auto key = _lexer.Lex();

    _lexer.SetDelimeters({"\r\n", "\r", "\n"});
    auto value = _lexer.Lex(false);

    _fields[{key.first, key.second}] = {value.first, value.second};
}

} // namespace Chili

