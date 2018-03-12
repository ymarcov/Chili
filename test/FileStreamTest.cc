#include <gmock/gmock.h>

#include "FileStream.h"
#include "SystemError.h"
#include "TestFileUtils.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <string>

using namespace ::testing;
using namespace std::literals;

namespace Nitra {

class FileStreamTest : public Test {
public:
    FileStreamTest() {
        _fd = OpenTempFile();
        WriteSomeInitialText(_textInFile);
        RewindFile();
    }

    ~FileStreamTest() {
        if (_fd != -1)
            ::close(_fd);
    }

protected:
    void ExpectDuplicateFileHandles(FileStream& lhs, FileStream& rhs) {
        ASSERT_LE(6, _textInFile.size()) << "Initial file text must be longer";

        auto fstPart = _textInFile.substr(0, 5);
        auto sndPart = _textInFile.substr(5);

        auto fstBuffer = std::array<char, 0x100>{};
        ASSERT_EQ(fstPart.size(), lhs.Read(fstBuffer.data(), fstPart.size()));

        auto sndBuffer = std::array<char, 0x100>{};
        ASSERT_EQ(sndPart.size(), rhs.Read(sndBuffer.data(), sndPart.size()));

        EXPECT_EQ(fstPart, std::string(fstBuffer.data(), fstPart.size()));
        EXPECT_EQ(sndPart, std::string(sndBuffer.data(), sndPart.size()));
    }

    int _fd;
    std::string _textInFile = "hello world!";

private:
    void RewindFile() {
        ::off_t result = ::lseek(_fd, 0, SEEK_SET);

        if (result == -1)
            throw SystemError{};

        if (result != 0)
            throw std::runtime_error("Did not rewind file to beginning");
    }

    void WriteSomeInitialText(const std::string& txt) {
        int result = ::write(_fd, txt.c_str(), txt.size());

        if (result == -1)
            throw SystemError{};

        if (result != static_cast<int>(txt.size()))
            throw std::runtime_error("Not all bytes were written to file");
    }
};

TEST_F(FileStreamTest, reads) {
    auto fs = FileStream{_fd};
    auto buffer = std::array<char, 0x100>{};

    ASSERT_EQ(_textInFile.size(), fs.Read(buffer.data(), buffer.size()));
    EXPECT_EQ(_textInFile, std::string(buffer.data(), _textInFile.size()));
}

TEST_F(FileStreamTest, reads_with_timeout_param) {
    auto fs = FileStream{_fd};
    auto buffer = std::array<char, 0x100>{};

    ASSERT_EQ(_textInFile.size(), fs.Read(buffer.data(), buffer.size(), 10ms));
    EXPECT_EQ(_textInFile, std::string(buffer.data(), _textInFile.size()));
}

TEST_F(FileStreamTest, write_seek_read) {
    auto fs = SeekableFileStream{_fd};
    auto txt = std::string{"**hello there**"};

    ASSERT_EQ(txt.size(), fs.Write(txt.data(), txt.size()));

    fs.Seek(0);

    auto buffer = std::array<char, 0x100>{};
    ASSERT_EQ(txt.size(), fs.Read(buffer.data(), buffer.size()));
    EXPECT_EQ(txt, std::string(buffer.data(), txt.size()));
}

TEST_F(FileStreamTest, writes_to_other_file) {
    auto src = FileStream{_fd};
    auto dest = SeekableFileStream{OpenTempFile()};

    src.WriteTo(dest, _textInFile.size());
    dest.Seek(0);

    auto buffer = std::array<char, 0x100>{};
    ASSERT_EQ(_textInFile.size(), dest.Read(buffer.data(), buffer.size()));
    EXPECT_EQ(_textInFile, std::string(buffer.data(), _textInFile.size()));
}

TEST_F(FileStreamTest, duplicate_handle_shares_position) {
    auto fs = FileStream{_fd};
    auto dup = FileStream{fs};

    ExpectDuplicateFileHandles(fs, dup);
}

TEST_F(FileStreamTest, lvalue_assignment_makes_duplicate) {
    auto assignee = FileStream{};

    auto fs = FileStream{_fd};
    auto origHandle = fs.GetNativeHandle();
    assignee = fs;

    EXPECT_EQ(origHandle, fs.GetNativeHandle());
    EXPECT_NE(assignee.GetNativeHandle(), origHandle);

    ExpectDuplicateFileHandles(fs, assignee);
}

TEST_F(FileStreamTest, rvalue_assignment) {
    auto assignee = FileStream{};

    auto fs = FileStream{_fd};
    auto origHandle = fs.GetNativeHandle();

    assignee = std::move(fs);

    // we don't want to duplicate the handle, but to take it
    EXPECT_EQ(FileStream::InvalidHandle, fs.GetNativeHandle());
    EXPECT_EQ(origHandle, assignee.GetNativeHandle());

    // check we got something that works after assignment
    auto buffer = std::array<char, 0x100>{};
    EXPECT_EQ(_textInFile.size(), assignee.Read(buffer.data(), buffer.size()));
    EXPECT_EQ(_textInFile, std::string(buffer.data(), _textInFile.size()));
}

} // namespace Nitra

