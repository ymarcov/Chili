#include <gmock/gmock.h>

#include "OrchestratedTcpServer.h"
#include "FileStream.h"
#include "Poller.h"
#include "TestFileUtils.h"
#include "WaitEvent.h"

#include <atomic>
#include <chrono>
#include <random>

using namespace ::testing;
using namespace std::literals;

namespace Yam {
namespace Http {

class OrchestratorTest : public Test {
public:
    OrchestratorTest() {
    }

protected:
    std::shared_ptr<SeekableFileStream> MakeFile(int minSize, int maxSize = -1) {
        auto fs = std::make_shared<SeekableFileStream>(OpenTempFile());
        WriteMoreData(fs, minSize, maxSize);
        return fs;
    }

    void WriteMoreData(std::shared_ptr<SeekableFileStream>& fs, int minSize, int maxSize = -1) {
        if (maxSize == -1)
            maxSize = minSize;

        std::random_device rd;
        std::uniform_int_distribution<int> dist(minSize, maxSize);
        auto bufferSize = dist(rd);
        auto buffer = std::vector<char>(bufferSize);

        for (int i = 0; i < bufferSize; i++)
            buffer[i] = dist(rd) % 0xFF;

        fs->Write(buffer.data(), buffer.size());
        fs->Seek(0);
    }
};

TEST_F(OrchestratorTest, one_client_header_only) {
}

} // namespace Http
} // namespace Yam

