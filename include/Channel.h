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
        Read,
        Process,
        WaitWritable,
        Write,
        Closed
    };

    struct Throttlers {
        struct {
            Throttler Dedicated;
            std::shared_ptr<Throttler> Master;
        } Read, Write;
    };

    Channel(std::shared_ptr<FileStream>, Throttlers);
    virtual ~Channel() = default;

protected:
    enum class Control {
        FetchBody,
        ResponseSent
    };

    virtual Control Process(Request&, Responder&) = 0;

    Control Send(Status);
    Control FetchBody();
    void ThrottleRead(Throttler);
    void ThrottleWrite(Throttler);

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
    std::chrono::time_point<std::chrono::steady_clock> GetTimeout() const;

    /**
     * Gets whether the channel is ready to perform its stage.
     */
    bool IsReady() const;

    void OnRead();
    void OnProcess();
    void OnWrite();
    void Close();

    const std::shared_ptr<FileStream>& GetStream() const;

    std::shared_ptr<FileStream> _stream;
    Throttlers _throttlers;
    std::chrono::time_point<std::chrono::steady_clock> _timeout = std::chrono::steady_clock::now();
    std::shared_ptr<void> _requestBuffer;
    Request _request;
    Responder _responder;
    mutable std::recursive_mutex _mutex;
    Stage _stage;
    bool _error = false;
    bool _fetchBody = false;
    std::vector<char> _body;
    std::size_t _bodyPosition;

    friend class Orchestrator;
};

} // namespace Http
} // namespace Yam

