#pragma once

#include "FileStream.h"

#include <atomic>
#include <memory>

namespace Yam {
namespace Http {

class Socket : public FileStream {
public:
    Socket();
    Socket(NativeHandle);
    Socket(const Socket&);
    Socket(Socket&&);

    Socket& operator=(const Socket&);
    Socket& operator=(Socket&&);

    std::size_t Write(const void* buffer, std::size_t maxBytes) override;
    std::size_t WriteTo(FileStream&, std::size_t maxBytes) override;

protected:
    static Socket& IncrementUseCount(Socket&);

    void Close() override;
    void Shutdown();

    std::shared_ptr<std::atomic_int> _useCount;
};

} // namespace Http
} // namespace Yam

