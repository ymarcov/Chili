#include <gmock/gmock.h>

#include "Request.h"
#include "TestUtils.h"

using namespace ::testing;

namespace Yam {
namespace Http {

const char testRequest_simple[] =
    "POST /test/uri HTTP/1.1\r\n"
    "User-Agent: curl/7.38.0\r\n"
    "Host: google.com\r\n"
    "Accept: */*\r\n"
    "\r\n"
    "Test body";

#define REQUEST(request) \
    PtrShim(testRequest_##request), sizeof(testRequest_##request)

TEST(RequestTest, parse_simple) {
    Request r;
    Request::Parse(&r, REQUEST(simple));

    EXPECT_EQ(HttpVersion::Http11, r.GetHttpVersion());
    EXPECT_EQ(Request::Method::Post, r.GetMethod());
    EXPECT_EQ("/test/uri", r.GetUri());
    EXPECT_EQ("curl/7.38.0", r.GetUserAgent());
    EXPECT_EQ("*/*", r.GetAccept());
    EXPECT_STREQ("Test body", r.GetBody());
}

} // namespace Http
} // namespace Yam

