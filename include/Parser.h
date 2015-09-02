#pragma once

#include <algorithm>
#include <cstddef>
#include <experimental/string_view>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <unordered_map>

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

    Field GetField(const std::string& name) const;
    Field GetMethod() const;
    Field GetUri() const;
    Field GetProtocolVersion() const;
    const char* GetBody() const;

private: // private functions
    void ParseRequestLine();
    Field ParseUntil(char delimeter);
    Field ParseRestOfLine();
    bool EndOfHeader() const;
    void ParseNextFieldLine();
    void SkipToBody();

private: // private aliases and helper types
    using string_view = std::experimental::string_view;

    struct CIHash {
        template <class S>
        std::size_t operator()(const S& val) const {
            return std::accumulate(begin(val), end(val), 0, [](char init, char c) {
                return init + std::tolower(c);
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
    const char* _positionedBuffer;
    std::size_t _remainingChars;
    std::unordered_map<int, Field> _requestLineFields;
    std::unordered_map<string_view, Field, CIHash, CICmp> _extraFields;
};

} // namespace Http
} // namespace Yam

