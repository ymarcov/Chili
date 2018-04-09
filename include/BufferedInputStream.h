#pragma once

#include "InputStream.h"
#include "Signal.h"

namespace Chili {

class BufferedInputStream : public InputStream {
public:
    virtual void BufferInputAsync() = 0;
    virtual std::size_t GetBufferedInputSize() const = 0;

    Signal<> OnInputBuffered;
};

} // namespace Chili

