#pragma once

#include <array>
#include <initializer_list>

namespace Yam {
namespace Http {

class Lexer {
public:
    Lexer(const char* stream, std::size_t length);

    void SetDelimeters(std::initializer_list<const char*>);
    std::size_t GetConsumption() const;
    std::pair<const char*, std::size_t> GetRemaining() const;
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

} // namespace Http
} // namespace Yam

