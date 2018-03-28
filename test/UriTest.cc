#include <gmock/gmock.h>

#include "Uri.h"

using namespace ::testing;

namespace Chili {

class UriTest : public Test {
public:
    UriTest() {
    }

protected:
};

TEST_F(UriTest, encode_decode) {
    auto str = std::string{"hello &there ~~!@#&%*@#)#$()%_)+/\\/"};
    ASSERT_EQ(str, Uri::Decode(Uri::Encode(str)));
}

} // namespace Chili

