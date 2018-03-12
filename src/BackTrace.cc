#include "BackTrace.h"

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cxxabi.h>
#include <string.h>
#include <execinfo.h>
#include <ostream>

namespace Nitra {

namespace {

std::vector<std::string>& SetFailed(std::vector<std::string>& vec) {
    vec.clear();
    vec.emplace_back("FAILED TO GET BACK TRACE");
    return vec;
}

std::string Demangle(const std::string& s) {
    if (s.back()) // not null terminated
        return s;

    std::size_t length;
    int status;
    char* buffer;

    buffer = abi::__cxa_demangle(s.data(), nullptr, &length, &status);

    std::string result = status ? s : buffer;

    std::free(buffer);

    return result;
}

} // unnamed namespace

BackTrace::BackTrace() {
    auto context = std::make_unique<::unw_context_t>();

    if (!::unw_getcontext(context.get())) {
        _context = std::move(context);
        GetFrames();
    }
}

BackTrace::~BackTrace() = default;

const std::vector<std::string>& BackTrace::GetFrames() const {
    if (!_context)
        return _frames;

    std::vector<std::string> result;

    ::unw_cursor_t cursor;

    if (int res = ::unw_init_local(&cursor, static_cast<::unw_context_t*>(_context.get()))) {
        printf("ERROR uil: %d\n", res);
        _context.reset();
        return SetFailed(_frames);
    }

    while (int res = ::unw_step(&cursor)) {
        if (res < 0) {
            _context.reset();
            printf("ERROR us: %d\n", res);
            return SetFailed(_frames);
        }

        std::array<char, 0x400> buffer;
        ::unw_word_t offset;

        if (!::unw_get_proc_name(&cursor, buffer.data(), buffer.size(), &offset)) {
            std::size_t nameLength = ::strnlen(buffer.data(), buffer.size());

            if (nameLength == buffer.size()) {
                buffer.back() = '\0';
                --nameLength;
            }

            auto s = Demangle(std::string{buffer.data(), nameLength + 1});
            s += " +";
            s += std::to_string(offset);

            result.push_back(std::move(s));
        } else {
            result.emplace_back("Unknown");
        }
    }

    _context.reset();

    return _frames = std::move(result);
}

std::ostream& operator<<(std::ostream& os, const BackTrace& bt) {
    os << "Back Trace:\n";
    os << "-----------\n";

    auto frames = bt.GetFrames();

    for (std::size_t i = 0; i < frames.size(); ++i)
        os << "  #" << i << ": " << frames[i] << "\n";

    return os;
}

} // namespace Nitra

