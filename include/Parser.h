#pragma once

#include "Lexer.h"

#include <algorithm>
#include <experimental/string_view>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <unordered_map>
#include <vector>

namespace Yam {
namespace Http {

class Parser {
public: // public types
    struct Error : public std::runtime_error {
        Error(const char* what) : runtime_error(what) {}
    };

    struct Field {
        const char* Data;
        std::size_t Size;
    };

public: // public functions
    Parser(const char* buf, std::size_t bufSize);

    void Parse();

    std::vector<Field> GetCookieNames() const;
    Field GetCookie(const std::string& name) const;
    Field GetField(const std::string& name) const;
    Field GetMethod() const;
    Field GetProtocolVersion() const;
    Field GetUri() const;
    const char* GetBody() const;
    std::size_t GetBodyLength() const;

private: // private functions
    void ParseCookies() const;
    void ParseNextFieldLine();
    void ParseRequestLine();
    bool EndOfHeader();
    void SkipToBody();

private: // private aliases and helper types
    using string_view = std::experimental::string_view;

    struct CIHash {
        template <class S>
        std::size_t operator()(const S& val) const {
            return std::accumulate(begin(val), end(val), 0, [](char agg, char c) {
                return agg + std::tolower(c);
            });
        }
    };

    struct CICmp {
        template <class S>
        bool operator()(const S& lhs, const S& rhs) const {
            return (lhs.size() == rhs.size()) &&
                !(::strncasecmp(lhs.data(), rhs.data(), lhs.size()));
        }
    };

private: // private variables
    Lexer _lexer;
    std::unordered_map<string_view, Field, CIHash, CICmp> _fields;
    mutable std::unordered_map<string_view, Field, CIHash, CICmp> _cookies;
    mutable bool _cookiesHaveBeenParsed = false;
};

} // namespace Http
} // namespace Yam

