#include <gmock/gmock.h>

#include "Parser.h"

#include <cstring>

using namespace ::testing;

namespace Yam {
namespace Http {

const char testRequest[] =
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
"\r\n"
"Request body!";

class ParserTest : public Test {
public:
    ParserTest() :
        p{testRequest, sizeof(testRequest)} {
        p.Parse();
    }

protected:
    Parser p;
};

TEST_F(ParserTest, request_line) {
    Parser::Field method, uri, protocolVersion;

    ASSERT_NO_THROW(method = p.GetMethod());
    EXPECT_EQ("GET", std::string(method.Data, method.Size));

    ASSERT_NO_THROW(uri = p.GetUri());
    EXPECT_EQ("/path/to/res", std::string(uri.Data, uri.Size));

    ASSERT_NO_THROW(protocolVersion = p.GetProtocolVersion());
    EXPECT_EQ("HTTP/1.1", std::string(protocolVersion.Data, protocolVersion.Size));
}

TEST_F(ParserTest, few_fields) {
    Parser::Field field;

    ASSERT_NO_THROW(field = p.GetField("Accept"));
    EXPECT_EQ("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            std::string(field.Data, field.Size));

    ASSERT_NO_THROW(field = p.GetField("Host"));
    EXPECT_EQ("request.urih.com",
            std::string(field.Data, field.Size));

    ASSERT_NO_THROW(field = p.GetField("X-real-ip"));
    EXPECT_EQ("95.35.33.46",
            std::string(field.Data, field.Size));
}

TEST_F(ParserTest, request_body) {
    EXPECT_STREQ("Request body!", p.GetBody());
}

TEST_F(ParserTest, case_insensitive_key) {
    Parser::Field field;
    ASSERT_NO_THROW(field = p.GetField("host"));
    EXPECT_EQ("request.urih.com", std::string(field.Data, field.Size));
}

TEST_F(ParserTest, cookie_raw) {
    Parser::Field field = p.GetField("cookie");
    EXPECT_EQ("Session=abcd1234; User=Yam", std::string(field.Data, field.Size));
}

TEST_F(ParserTest, cookie_get_specific) {
    Parser::Field session = p.GetCookie("Session");
    EXPECT_EQ("abcd1234", std::string(session.Data, session.Size));

    Parser::Field user = p.GetCookie("user"); // note intentional wrong case
    EXPECT_EQ("Yam", std::string(user.Data, user.Size));

    ASSERT_THROW(p.GetCookie("unspecified"), std::runtime_error);
}

} // namespace Http
} // namespace Yam

