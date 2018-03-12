#include "Uri.h"

#include <curl/curl.h>
#include <stdexcept>

namespace Nitra {

namespace {

class CurlGlobalSession {
public:
    CurlGlobalSession() {
        if (!(_curl = ::curl_easy_init()))
            throw std::runtime_error("CURL failed to initialize!");
    }

    ~CurlGlobalSession() {
        ::curl_easy_cleanup(_curl);
    }

    operator CURL*() const {
        return _curl;
    }

private:
    CURL* _curl;
} CurlGlobal;

} // unnamed namespace

std::string Uri::Encode(const std::string& s) {
     auto result = ::curl_easy_escape(CurlGlobal, s.data(), s.size());

     if (!result)
         throw std::runtime_error("curl_easy_escape() failed");

     auto str = std::string{result};
     ::curl_free(result);

     return str;
}

std::string Uri::Decode(const std::string& s) {
    int outLength;
    auto result = ::curl_easy_unescape(CurlGlobal, s.data(), s.size(), &outLength);

    if (!result)
        throw std::runtime_error("curl_easy_unescape() failed");

    auto str = std::string{result, static_cast<unsigned>(outLength)};
    ::curl_free(result);

    return str;
}

} // namespace Nitra

