#pragma once

#include <array>
#include <initializer_list>

namespace Nitra {

/*
 * A low-overhead lexer which lets you specify and change
 * delimeters on-the-fly as you go along, and extract extra tokens.
 *
 * Specs
 * -----
 * N = Input length
 * Memory: O(1)
 * Runtime: O(N)
 */
class Lexer {
public:
    /*
     * Creates a degenerate object.
     */
    Lexer() = default;

    /*
     * Creates a lexer for parsing the specified stream.
     */
    Lexer(const char* stream, std::size_t length);

    /*
     * Sets the delimeters used for tokenizing.
     * Can be called multiple time during one session.
     */
    void SetDelimeters(std::initializer_list<const char*>);

    /*
     * Gets how much of the stream has already been consumed.
     */
    std::size_t GetConsumption() const;

    /*
     * Gets a pointer to the current position in the stream,
     * along with how many bytes have not yet been consumed.
     */
    std::pair<const char*, std::size_t> GetRemaining() const;

    /*
     * Returns true of the end of the stream has been reached.
     */
    bool EndOfStream() const;

    /*
     * Skips the delimeters currently ahead of the stream.
     * Returns how many bytes were skipped in the process.
     */
    std::size_t SkipDelimeters();

    /*
     * Returns a pointer to the next token, along with its size.
     */
    std::pair<const char*, std::size_t> Lex(bool skipFollowingDelimeters = true);

private:
    std::size_t DistanceToNextToken(const char* cursor, std::size_t consumed);
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

} // namespace Nitra

