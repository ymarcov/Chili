#pragma once

#include "FileStream.h"

#include <atomic>
#include <memory>
#include <vector>

namespace Chili {

class SocketStream : public FileStream {
public:
    SocketStream();
    SocketStream(NativeHandle);
    SocketStream(const SocketStream&);
    SocketStream(SocketStream&&);

    SocketStream& operator=(const SocketStream&);
    SocketStream& operator=(SocketStream&&);

    std::size_t Write(const void* buffer, std::size_t maxBytes) override;
    std::size_t WriteVector(std::vector<std::pair<const void*, std::size_t>>) override;
    std::size_t WriteTo(FileStream&, std::size_t maxBytes) override;

protected:
    static SocketStream& IncrementUseCount(SocketStream&);

    void Close() override;
    void Shutdown();

    std::shared_ptr<std::atomic_int> _useCount;
};

} // namespace Chili

