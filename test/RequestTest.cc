#include <gmock/gmock.h>

#include "Request.h"
#include "MemoryPool.h"

#include <cstring>

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
"Content-Length: 15\r\n"
"\r\n"
"Request body!";

class RequestTest : public Test {
public:
    RequestTest() :
        _memoryPool{MemoryPool<Request::Buffer>::Create(4)} {}

protected:
    std::shared_ptr<MemoryPool<Request::Buffer>> _memoryPool;
};

TEST_F(RequestTest, getters) {
    auto buffer = _memoryPool->New();
    std::strncpy(buffer.get(), requestData, sizeof(requestData));

    Request r{std::move(buffer)};

    EXPECT_EQ("GET", r.GetMethod());
    EXPECT_EQ("/path/to/res", r.GetUri());
    EXPECT_EQ("HTTP/1.1", r.GetProtocol());
    EXPECT_EQ("request.urih.com", r.GetField("Host"));
    EXPECT_EQ("abcd1234", r.GetCookie("Session"));
    EXPECT_EQ(2, r.GetCookieNames().size());

    auto body = r.GetBody();
    EXPECT_EQ("Request body!", std::string(body.first, body.second));
}

} // namespace Http
} // namespace Yam

