#include <gmock/gmock.h>

#include "Profiler.h"

#include <fmtlib/format.h>

using namespace ::testing;

namespace Nitra {

class ProfilerTest : public Test {
public:
    struct Data {
        static std::shared_ptr<Data> Make(std::string text, int number) {
            auto data = std::make_shared<Data>();
            data->Text = std::move(text);
            data->Number = number;
            return data;
        }

        std::string Text;
        int Number;
    };

    ProfilerTest() {
    }

protected:
};

TEST_F(ProfilerTest, record_and_read) {
    Profiler p;

    p.Record<GenericProfileEvent>("Test Source", Data::Make("hello", 123));

    struct Reader : ProfileEventReader {
        void Read(const GenericProfileEvent& pe) {
            Source = pe.GetSource();

            auto data = std::static_pointer_cast<const ProfilerTest::Data>(pe.Data);
            Text = data->Text;
            Number = data->Number;
        }

        std::string Source;
        std::string Text;
        int Number;
    } reader;

    auto events = p.GetEvents();

    ASSERT_EQ(1, events.size());

    reader.Visit(events[0]);

    EXPECT_EQ("Test Source", reader.Source);
    EXPECT_EQ("hello", reader.Text);
    EXPECT_EQ(123, reader.Number);
}

} // namespace Nitra

