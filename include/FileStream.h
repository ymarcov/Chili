#pragma once

#include "InputStream.h"
#include "OutputStream.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace Nitra {

enum FileMode {
    Read = 0x1,
    Write = 0x2,
    Append = 0x4,
    Create = 0x8,
    Truncate = 0x10
};

class FileStream : public InputStream, public OutputStream {
public:
    using NativeHandle = int;

    static std::unique_ptr<FileStream> Open(const std::string& path, FileMode mode);

    static const NativeHandle InvalidHandle;

    FileStream();
    FileStream(NativeHandle);
    FileStream(const FileStream&);
    FileStream(FileStream&&);

    ~FileStream() override;

    FileStream& operator=(const FileStream&);
    FileStream& operator=(FileStream&&);

    NativeHandle GetNativeHandle() const;
    void SetBlocking(bool);

    std::size_t Read(void* buffer, std::size_t maxBytes) override;
    std::size_t Read(void* buffer, std::size_t maxBytes, std::chrono::milliseconds timeout) override;
    std::size_t Write(const void* buffer, std::size_t maxBytes) override;
    virtual std::size_t WriteTo(FileStream&, std::size_t maxBytes);

    bool EndOfStream() const override;

protected:
    virtual void Close();

    std::atomic<NativeHandle> _nativeHandle;
    bool _endOfStream = false;
};

class SeekableFileStream : public FileStream {
public:
    using FileStream::FileStream;

    void Seek(std::size_t offset);
};

} // namespace Nitra

