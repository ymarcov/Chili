#include <gmock/gmock.h>

#include "Responder.h"

#include <sstream>

using namespace ::testing;

namespace Yam {
namespace Http {

namespace {

class StringOutputStream : public OutputStream {
public:
    void Write(const void* buffer, std::size_t bufferSize) override {
        _stream.write(static_cast<const char*>(buffer), bufferSize);
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
    auto MakeResponder(std::shared_ptr<StringOutputStream> s) {
        return std::make_unique<Responder>(s);
    }

    auto MakeStream() {
        return std::make_shared<StringOutputStream>();
    }
};

TEST_F(ResponderTest, responds_with_continue) {
    auto stream = MakeStream();
    auto r = MakeResponder(stream);

    r->Send(Protocol::Status::Continue);

    EXPECT_EQ("HTTP/1.1 100 Continue\r\n\r\n", stream->ToString());
}

} // namespace Http
} // namespace Yam

