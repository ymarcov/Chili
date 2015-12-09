#include <gmock/gmock.h>

#include "Responder.h"

#include <sstream>

using namespace ::testing;

namespace Yam {
namespace Http {

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

} // unnamed namespace

class ResponderTest : public Test {
protected:
    auto MakeResponder(std::shared_ptr<StringOutputStream> s) const {
        return std::make_unique<Responder>(std::move(s));
    }

    auto MakeStream() const {
        return std::make_shared<StringOutputStream>();
    }

    auto MakeBodyFromString(const std::string& s) const {
        return std::make_shared<std::vector<char>>(s.data(), s.data() + s.size());
    }
};

TEST_F(ResponderTest, just_status) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->Send(Status::Continue);

    EXPECT_EQ("HTTP/1.1 100 Continue\r\n\r\n", stream->ToString());
}

TEST_F(ResponderTest, some_headers) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->SetField("First", "Hello world!");
    r->SetField("Second", "v4r!0u$ sYm80;5");
    r->Send(Status::Ok);

    auto expected = "HTTP/1.1 200 OK\r\n"
        "First: Hello world!\r\n"
        "Second: v4r!0u$ sYm80;5\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, headers_and_body) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->SetBody(MakeBodyFromString("Hello world!"));
    r->Send(Status::BadGateway);

    auto expected = "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello world!";

    EXPECT_EQ(expected, stream->ToString());
}

TEST_F(ResponderTest, simple_cookies) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->SetCookie("First", "One");
    r->SetCookie("Second", "Two");
    r->Send(Status::NotFound);

    auto expected = "HTTP/1.1 404 Not Found\r\n"
        "Set-Cookie: First=One\r\n"
        "Set-Cookie: Second=Two\r\n"
        "\r\n";

    EXPECT_EQ(expected, stream->ToString());
}

} // namespace Http
} // namespace Yam

