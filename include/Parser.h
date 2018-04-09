#pragma once

#include "Lexer.h"

#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <strings.h>
#include <unordered_map>
#include <vector>

namespace Chili {

/*
 * Parses an HTTP request efficiently.
 * Avoids dynamic allocations and copying of strings.
 */
class Parser {
public: // public types
    class Error : public std::runtime_error {
    public:
        enum class Type {
            None,
            Malformed,
        };

        Error(const char* what);
        Error(Type, const char* what = nullptr);

        Type GetType() const;

    private:
        Type _type;
    };

    struct Field {
        const char* Data;
        std::size_t Size;
    };

public: // public functions
    /*
     * Creates a parser for the specified stream.
     * This is the main, useful creation method.
     */
    static Parser Parse(const char* buf, std::size_t bufSize);

    /*
     * Creates a degenerate object.
     */
    Parser() = default;

    std::vector<Field> GetFieldNames() const;
    std::vector<Field> GetCookieNames() const;
    Field GetCookie(const std::string_view& name) const;
    Field GetField(const std::string_view& name) const;
    bool GetField(const std::string_view& name, Field* value) const;
    Field GetMethod() const;
    Field GetVersion() const;
    Field GetUri() const;
    std::size_t GetHeaderLength() const;
    const char* GetBody() const;

private: // private aliases and helper types
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

    using InternalHash = std::unordered_map<std::string_view, Field, CIHash, CICmp>;

private: // private functions
    Parser(const char* buf, std::size_t bufSize);

    void ParseAll();
    void ParseCookies() const;
    void ParseNextFieldLine();
    void ParseRequestLine();
    bool EndOfHeader();
    void SkipToBody();

private: // private variables
    Lexer _lexer;
    InternalHash _fields;
    mutable InternalHash _cookies;
    mutable bool _cookiesHaveBeenParsed = false;
};

} // namespace Chili

