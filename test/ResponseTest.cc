#include <gmock/gmock.h>

#include "Response.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstring>
#include <sstream>
#include <time.h> // some functions aren't in <ctime>

using namespace ::testing;

namespace Chili {

namespace {

class StringOutputStream : public OutputStream {
public:
    std::size_t Write(const void* buffer, std::size_t bufferSize) override {
        _stream.write(static_cast<const char*>(buffer), bufferSize);
        return bufferSize;
    }

    std::string ToString() const {
        return _stream.str();
    }

private:
    std::ostringstream _stream;
};

class ChunkedStringInputStream : public InputStream {
public:
    ChunkedStringInputStream(std::vector<std::string> chunks) :
        _chunks(std::move(chunks)) {}

    std::size_t Read(void* buffer, std::size_t size) override {
        auto& chunk = _chunks[_currentChunk];
        std::strncpy(static_cast<char*>(buffer), chunk.data(), size);
        ++_currentChunk;
        return std::min(size, chunk.size());
    }

    bool EndOfStream() const override {
        return _currentChunk == _chunks.size();
    }

    std::string ToString() const {
        std::ostringstream stream;
        for (auto& c : _chunks)
            stream << c;
        return stream.str();
    }

private:
    std::vector<std::string> _chunks;
    std::size_t _currentChunk = 0;
};

} // unnamed namespace

class ResponseTest : public Test {
protected:
    auto MakeResponse(std::shared_ptr<StringOutputStream> s) const {
        return std::make_unique<Response>(std::move(s));
    }

    auto MakeStream() const {
        return std::make_shared<StringOutputStream>();
    }

    auto MakeBodyFromString(const std::string& s) const {
        return std::make_shared<std::vector<char>>(s.data(), s.data() + s.size());
    }

    auto MakeChunkedStream(std::vector<std::string> chunks) {
        return std::make_shared<ChunkedStringInputStream>(std::move(chunks));
    }

    void Flush(std::unique_ptr<Response>& r, std::size_t quota = 1) {
        std::size_t consumed;
        while (!r->Flush(quota, consumed))
            ;
    }
};

TEST_F(ResponseTest, just_status) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    r->Send(Status::Continue);
    Flush(r);

    EXPECT_EQ("HTTP/1.1 100 Continue\r\nContent-Length: 0\r\n\r\n", stream->ToString());
}

TEST_F(ResponseTest, some_headers) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    r->AppendField("First", "Hello world!");
    r->AppendField("Second", "v4r!0u$ sYm80;5");
    r->Send(Status::Ok);
    Flush(r);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "First: Hello world!\r\n"
        "Second: v4r!0u$ sYm80;5\r\n"
	"Content-Length: 0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, headers_and_body) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    r->SetContent(MakeBodyFromString("Hello world!"));
    r->Send(Status::BadGateway);
    Flush(r);

    auto expected = "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello world!";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, headers_and_body_as_string) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    r->SetContent("Hello world!");
    r->Send(Status::BadGateway);
    Flush(r);

    auto expected = "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello world!";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, simple_cookies) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    r->SetCookie("First", "One");
    r->SetCookie("Second", "Two");
    r->Send(Status::NotFound);
    Flush(r);

    auto expected = "HTTP/1.1 404 Not Found\r\n"
        "Set-Cookie: First=One\r\n"
        "Set-Cookie: Second=Two\r\n"
	"Content-Length: 0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, cookie_with_simple_options) {
    using namespace std::literals;

    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    CookieOptions opts;
    opts.SetDomain("example.com");
    opts.SetPath("/some/path");
    opts.SetMaxAge(10min);

    r->SetCookie("First", "One", opts);
    r->Send(Status::Ok);
    Flush(r);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; Domain=example.com; Path=/some/path; Max-Age=600\r\n"
	"Content-Length: 0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, cookie_with_expiration_date) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    struct tm tm;
    if (!::strptime("2013-01-15 21:47:38", "%Y-%m-%d %T", &tm))
        throw std::logic_error("Bad call to strptime()");
    auto t = ::timegm(&tm);

    CookieOptions opts;
    opts.SetExpiration(t);

    r->SetCookie("First", "One", opts);
    r->Send(Status::Ok);
    Flush(r);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; Expires=Tue, 15 Jan 2013 21:47:38 GMT\r\n"
	"Content-Length: 0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, cookie_with_httponly_and_secure) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    CookieOptions opts;
    opts.SetHttpOnly();
    opts.SetSecure();

    r->SetCookie("First", "One", opts);
    r->Send(Status::Ok);
    Flush(r);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; HttpOnly; Secure\r\n"
	"Content-Length: 0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, send_cached) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    CookieOptions opts;
    opts.SetHttpOnly();
    opts.SetSecure();
    r->SetCookie("First", "One", opts);
    auto cached = r->CacheAs(Status::Ok);

    r = MakeResponse(stream);
    r->SendCached(cached);
    Flush(r);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: First=One; HttpOnly; Secure\r\n"
	"Content-Length: 0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponseTest, chunked) {
    auto stream = MakeStream();
    auto r = MakeResponse(stream);

    auto data = MakeChunkedStream({
        "<b>",
        "hello ",
        "world",
        "</b>"
    });

    r->SetContent(data);
    r->Send(Status::Ok);
    Flush(r, 0x1000);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "3\r\n"
        "<b>\r\n"
        "6\r\n"
        "hello \r\n"
        "5\r\n"
        "world\r\n"
        "4\r\n"
        "</b>\r\n"
        "0\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

} // namespace Chili

