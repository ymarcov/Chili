#pragma once

#include "InputStream.h"
#include "OutputStream.h"

#include <atomic>

namespace Yam {
namespace Http {

class FileStream : public InputStream, public OutputStream {
public:
    using NativeHandle = int;
    static const NativeHandle InvalidHandle;

    FileStream();
    FileStream(NativeHandle);
    FileStream(const FileStream&);
    FileStream(FileStream&&);

    ~FileStream() override;

    FileStream& operator=(const FileStream&);
    FileStream& operator=(FileStream&&);

    NativeHandle GetNativeHandle() const;

    std::size_t Read(void* buffer, std::size_t maxBytes) override;
    std::size_t Write(const void* buffer, std::size_t maxBytes) override;
    virtual std::size_t WriteTo(FileStream&, std::size_t maxBytes);

protected:
    virtual void Close();

    std::atomic<NativeHandle> _nativeHandle;
};

class SeekableFileStream : public FileStream {
public:
    using FileStream::FileStream;

    void Seek(std::size_t offset);
};

} // namespace Http
} // namespace Yam

