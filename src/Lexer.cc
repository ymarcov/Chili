#include "Lexer.h"
#include <string.h>

namespace Yam {
namespace Http {

Lexer::Lexer(const char* stream, std::size_t length) :
    _stream{stream},
    _initialLength{length},
    _length{length} {}

void Lexer::SetDelimeters(std::initializer_list<const char*> delimeters) {
    if (_delimeters.size() < delimeters.size())
        throw std::logic_error("Too many delimeters");

    _delimeterCount = 0;
    _usableDelimeterData = _delimeterData.size();

    for (const char* d : delimeters)
        AddDelimeter(d);
}

std::pair<const char*, std::size_t> Lexer::GetRemaining() const {
    return {_stream, _length};
}

std::size_t Lexer::GetConsumption() const {
    return _initialLength - _length;
}

bool Lexer::EndOfStream() const {
    return !_length;
}

std::size_t Lexer::SkipDelimeters() {
    std::size_t stride = DistanceToNextToken(_stream, 0);
    _stream += stride;
    _length -= stride;
    return stride;
}

std::pair<const char*, std::size_t> Lexer::Next(bool skipFollowingDelimeters) {
    const char* startingPoint = _stream;
    const char* cursor = startingPoint;
    std::size_t wordLength = 0;

    while (_length > wordLength) {
        if (std::size_t delimLength = DelimeterAt(cursor, wordLength)) {
            std::size_t stride = wordLength;

            if (skipFollowingDelimeters)
                stride += DistanceToNextToken(cursor, wordLength);
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

std::size_t Lexer::DistanceToNextToken(const char* cursor, std::size_t consumed) {
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

void Lexer::AddDelimeter(const char* delimeter) {
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

std::size_t Lexer::DelimeterAt(const char* cursor, std::size_t consumed) {
    for (std::size_t i = 0; i < _delimeterCount; ++i)
        if (_length > consumed + (_delimeterLengths[i] - 1))
            if (!::strncmp(cursor, _delimeters[i], _delimeterLengths[i]))
                return _delimeterLengths[i];
    return 0;
}

} // namespace Http
} // namespace Yam

