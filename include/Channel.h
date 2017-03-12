#pragma once

#include "Poller.h"
#include "Request.h"
#include "Responder.h"
#include "Signal.h"
#include "Throttler.h"

#include <mutex>

namespace Yam {
namespace Http {

class Channel {
public:
    enum class Stage {
        WaitReadable,
        WaitReadTimeout,
        Read,
        Process,
        WaitWritable,
        WaitWriteTimeout,
        Write,
        Closed
    };

    struct Throttlers {
        struct {
            Throttler Dedicated;
            std::shared_ptr<Throttler> Master;
        } Read, Write;
    };

    enum class Control {
        FetchContent,
        RejectContent,
        SendResponse
    };

    Channel(std::shared_ptr<FileStream>, Throttlers);
    virtual ~Channel();

    /**
     * The following public functions are designed to be
     * used from within the Process() implementation.
     */
    Request& GetRequest();
    Responder& GetResponder();
    Control FetchContent();
    Control RejectContent();
    Control SendResponse(Status);
    bool IsReadThrottled() const;
    bool IsWriteThrottled() const;
    void ThrottleRead(Throttler);
    void ThrottleWrite(Throttler);

protected:
    virtual Control Process() = 0;

private:
    /**
     * Take the next step in the state machine.
     */
    void PerformStage();

    /**
     * Gets and sets the stage the channel is currently in.
     */
    Stage GetStage() const;
    void SetStage(Stage);

    /**
     * If the channel is in a waiting stage, gets the timeout
     * to wait for before performing another stage, even
     * if data is already available.
     */
    std::chrono::time_point<std::chrono::steady_clock> GetRequestedTimeout() const;

    /**
     * Gets whether the channel is ready to perform its stage.
     */
    bool IsReady() const;

    /**
     * Gets whether the channel is waiting, either for IO or a timeout.
     */
    bool IsWaiting() const;

    void OnRead();
    void OnProcess();
    void OnWrite();
    void Close();

    const std::shared_ptr<FileStream>& GetStream() const;
    int GetId() const;

    std::uint64_t _id;
    std::shared_ptr<FileStream> _stream;
    Throttlers _throttlers;
    Request _request;
    Responder _responder;
    std::chrono::time_point<std::chrono::steady_clock> _timeout;
    Stage _stage;
    bool _error = false;
    bool _fetchContent = false;

    friend class Orchestrator;
};

} // namespace Http
} // namespace Yam

