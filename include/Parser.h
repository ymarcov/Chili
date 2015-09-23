#pragma once

#include <algorithm>
#include <array>
#include <experimental/string_view>
#include <initializer_list>
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
    Field GetBody() const;

private: // private functions
    void ParseCookies() const;
    void ParseNextFieldLine();
    void ParseRequestLine();
    bool EndOfHeader();
    void SkipToBody();

private: // private aliases and helper types
    using string_view = std::experimental::string_view;

    class Lexer {
    public:
        Lexer(const char* stream, std::size_t length);

        void SetDelimeters(std::initializer_list<const char*>);
        std::size_t GetConsumption() const;
        bool EndOfStream() const;
        std::size_t Compress();
        std::pair<const char*, std::size_t> Next(bool compress = true);

    private:
        std::size_t ConsumeDelimeters(const char* cursor, std::size_t consumed);
        void AddDelimeter(const char*);
        std::size_t DelimeterAt(const char* cursor, std::size_t consumed);

        const char* _stream;
        std::size_t _initialLength;
        std::size_t _length;
        std::array<const char*, 16> _delimeters;
        std::array<std::size_t, 16> _delimeterLengths;
        std::array<char, 64> _delimeterData;
        std::size_t _delimeterCount = 0;
        std::size_t _usableDelimeterData = 0;
    };

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
    const char* _positionedBuffer;
    std::size_t _remainingChars;
    std::unordered_map<string_view, Field, CIHash, CICmp> _extraFields;
    mutable std::unordered_map<string_view, Field, CIHash, CICmp> _cookies;
    mutable bool _cookiesHaveBeenParsed = false;
};

} // namespace Http
} // namespace Yam

