#include <gmock/gmock.h>

#include "Request.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace ::testing;

namespace Yam {
namespace Http {

const char requestData[] =
"GET /path/to/res HTTP/1.1\r\n"
"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
"Accept-encoding: gzip, deflate\r\n"
"Accept-language: en-US,en;q=0.5\r\n"
"Connection: close\r\n"
"Host: request.urih.com\r\n"
"Referer: http://www.google.com/?url=http%3A%2F%2Frequest.urih.com\r\n"
"User-agent: Mozilla/5.0 (X11; Linux x86_64; rv:31.0) Gecko/20100101 Firefox/31.0 Iceweasel/31.8.0\r\n"
"Cookie: Session=abcd1234; User=Yam\r\n"
"X-http-proto: HTTP/1.1\r\n"
"X-log-7527: 95.35.33.46\r\n"
"X-real-ip: 95.35.33.46\r\n"
"Content-Length: 13\r\n"
"\r\n"
"Request body!";

const char requestHeaderData[] =
"GET /path/to/res HTTP/1.1\r\n"
"User-agent: Mozilla/5.0 (X11; Linux x86_64; rv:31.0) Gecko/20100101 Firefox/31.0 Iceweasel/31.8.0\r\n"
"Cookie: Session=abcd1234; User=Yam\r\n"
"Connection: Keep-alive\r\n"
"Expect: 100-continue\r\n"
"Content-Length: 13\r\n"
"\r\n";

const char requestMissingEndOfHeaderData[] =
"GET /path/to/res HTTP/1.1\r\n"
"User-agent: Mozilla/5.0 (X11; Linux x86_64; rv:31.0) Gecko/20100101 Firefox/31.0 Iceweasel/31.8.0\r\n"
"Cookie: Session=abcd1234; User=Yam\r\n"
"Expect: 100-continue\r\n"
"Content-Length: 13\r\n";

const char requestBodyData[] = "Request body!";

namespace {

template <std::size_t N>
constexpr std::size_t NonTerminatedSize(const char (&s)[N]) {
    return s[N - 1] ? N : N - 1;
}

template <std::size_t N>
std::string ToString(const char (&s)[N]) {
    return {s, NonTerminatedSize(s)};
}
class StringInputStream : public InputStream {
public:
    StringInputStream() = default;

    StringInputStream(const std::string& str) :
        _str(str) {}

    std::size_t Read(void* buffer, std::size_t bufferSize) override {
        auto readBytes = std::min(Remaining(), bufferSize);
        std::memcpy(buffer, _str.data() + _position, readBytes);
        _position += readBytes;
        return readBytes;
    }

    bool EndOfStream() const override {
        return false; // ignored here
    }

    std::size_t Remaining() const {
        return _str.size() - _position;
    }

private:
    std::string _str;
    std::size_t _position = 0;
};

class NonContiguousInputStream : public InputStream {
public:
    NonContiguousInputStream(std::initializer_list<std::string> strings) {
        for (auto& s : strings)
            _streams.emplace_back(s);
    }

    std::size_t Read(void* buffer, std::size_t bufferSize) override {
        auto bytesRead = _streams.front().Read(buffer, bufferSize);

        if (!_streams.front().Remaining())
            _streams.erase(begin(_streams));

        return bytesRead;
    }

    bool EndOfStream() const override {
        return false; // ignored here
    }

private:
    std::vector<StringInputStream> _streams;
};

} // unnamed namespace

class RequestTest : public Test {
protected:
    auto MakeRequest(std::shared_ptr<InputStream>&& s) {
        auto req = std::make_unique<Request>(std::move(s));

        while (!req->ConsumeHeader(16).first)
            ;

        return req;
    }

    template <std::size_t N>
    auto MakeInputStream(const char (&str)[N]) const {
        return std::make_shared<StringInputStream>(ToString(str));
    }

    auto MakeContiguousInputStream() const {
        return MakeInputStream(requestData);
    }

    auto MakeNonContiguousInputStream() const {
        auto streams = { ToString(requestHeaderData), ToString(requestBodyData) };
        return std::make_shared<NonContiguousInputStream>(streams);
    }
};

TEST_F(RequestTest, header_getters) {
    auto r = MakeRequest(MakeContiguousInputStream());

    EXPECT_EQ(Method::Get, r->GetMethod());
    EXPECT_EQ(Version::Http11, r->GetVersion());
    EXPECT_EQ("/path/to/res", r->GetUri());
    EXPECT_EQ("request.urih.com", r->GetField("Host"));
    EXPECT_EQ("abcd1234", r->GetCookie("Session"));
    EXPECT_EQ(2, r->GetCookieNames().size());
    EXPECT_EQ(13, r->GetContentLength());
    EXPECT_FALSE(r->KeepAlive());
}

TEST_F(RequestTest, body) {
    auto r = MakeRequest(MakeContiguousInputStream());

    auto result = r->ConsumeContent(0x1000);

    EXPECT_TRUE(result.first);
    EXPECT_EQ("Request body!", std::string(r->GetContent().data(), r->GetContent().size()));
}

TEST_F(RequestTest, non_contiguous_header_and_body) {
    auto r = MakeRequest(MakeNonContiguousInputStream());
    auto result = r->ConsumeContent(0x1000);

    EXPECT_TRUE(result.first);
    EXPECT_EQ(Method::Get, r->GetMethod());
    EXPECT_EQ(Version::Http11, r->GetVersion());
    EXPECT_EQ("/path/to/res", r->GetUri());
    EXPECT_EQ(13, r->GetContentLength());
    EXPECT_EQ("100-continue", r->GetField("Expect"));
    EXPECT_EQ(NonTerminatedSize(requestBodyData), r->GetContent().size());
    EXPECT_TRUE(r->KeepAlive());
}

TEST_F(RequestTest, invalid_header_throws) {
    char buffer[0x2000] = {0};
    std::fill(std::begin(buffer), std::end(buffer), 1);
    EXPECT_THROW(MakeRequest(MakeInputStream(buffer)), std::runtime_error);
}

TEST_F(RequestTest, missing_end_of_header_in_small_buffer_never_finishes_consuming) {
    Request req(MakeInputStream(requestMissingEndOfHeaderData));

    EXPECT_FALSE(req.ConsumeHeader(-1).first);
    EXPECT_FALSE(req.ConsumeHeader(-1).first);
    EXPECT_FALSE(req.ConsumeHeader(-1).first);
}

} // namespace Http
} // namespace Yam

