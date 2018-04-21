#pragma once

#include "Clock.h"
#include "Poller.h"
#include "Profiler.h"
#include "Request.h"
#include "Response.h"
#include "Signal.h"
#include "Synchronized.h"
#include "Throttler.h"

#include <functional>
#include <memory>
#include <mutex>

namespace Chili {

/**
 * Channel
 */

class Channel : public std::enable_shared_from_this<Channel> {
public:
    /**
     * @internal
     */
    enum class Stage {
        WaitReadable,
        ReadTimeout,
        Read,
        Process,
        WaitWritable,
        WriteTimeout,
        Write,
        Writing,
        Closed
    };

    /**
     * @internal
     */
    struct Throttlers {
        struct Group {
            Throttler Dedicated;
            std::shared_ptr<Throttler> Master;
        } Read, Write;
    };

    Channel(std::shared_ptr<FileStream>);
    virtual ~Channel();

    /**
     * Gets a shared pointer to this channel.
     */
    std::shared_ptr<const Channel> GetSharedPointer() const;
    std::shared_ptr<Channel> GetSharedPointer();

    /**
     * Sets whether to automatically get each request's entire
     * message content, regardless of how long it is, before
     * calling Process().
     *
     * This is true by default.
     *
     * This may only be called in the channel's constructor.
     * Otherwise, the behavior is undefined.
     */
    void SetAutoFetchContent(bool);

    /**
     * Gets the request associated with the current processing call.
     */
    const Request& GetRequest() const;

    /**
     * Gets the response associated with the current processing call.
     */
    Response& GetResponse();

    /**
     * Instructs the server to fetch the rest of the content
     * (message body) of the request being processed.
     */
    void FetchContent(std::function<void()> callback);

    /**
     * Instructs the server to reject the rest of the content
     * (message body) of the request being processed.
     *
     * This is useful if, just judging by the header,
     * you know you will not be able to process the message
     * for one reason or another (perhaps only temporarily).
     */
    void RejectContent();

    /**
     * Sends the response back to the client.
     *
     * The actual response can be customized by configuring
     * the response properties -- which you can access by
     * calling GetResponse().
     */
    void SendResponse();

    /**
     * Returns true if reading is currently being throttled
     * on this channel.
     */
    bool IsReadThrottled() const;

    /**
     * Returns true if writing is currently being throttled
     * on this channel.
     */
    bool IsWriteThrottled() const;

    /**
     * Specifies a throttler to use for read operations
     * on this channel.
     *
     * This is particularly useful if the client intends
     * to send a very big request body, or perhaps makes
     * a lot of requests on this particular channel.
     */
    void ThrottleRead(Throttler);

    /**
     * Specifies a throttler to use for write operations
     * on this channel.
     *
     * This is particularly useful if you intend to send
     * the client a very big response body, or perhaps
     * engage in many back and forth request-response
     * sequences on this particular channel.
     */
    void ThrottleWrite(Throttler);

protected:
    /**
     * This function is called whenever the channel
     * has new data to process. This normally happens
     * when a new request has just been accepted and parsed,
     * but might also be called if, right after the request
     * header has been processed, the first call to Process()
     * requests that the rest of the message's body be retrieved.
     * Once it is retrieved, this function will be called a 2nd time.
     */
    virtual void Process() = 0;

private:
    struct ThrottlingInfo {
        std::size_t currentQuota;
        std::size_t capacity;
        bool full;
        Clock::TimePoint fillTime;
    };

    void Initialize(const std::shared_ptr<class Orchestrator>&);

    ThrottlingInfo GetThrottlingInfo(const Throttlers::Group&) const;

    /**
     * Take the next step in the state machine.
     */
    void Advance();

    /**
     * Gets and sets the stage the channel is currently in.
     */
    Stage GetTentativeStage() const;
    Stage GetDefiniteStage() const;
    void SetStage(Stage);

    /**
     * If the channel is in a waiting stage, gets the timeout
     * to wait for before performing another stage, even
     * if data is already available.
     */
    Clock::TimePoint GetRequestedTimeout() const;

    /**
     * Gets whether the channel is ready to perform its stage.
     */
    bool IsReady() const;

    /**
     * Gets whether the channel cannot make progress
     * until the client does something.
     */
    bool IsWaitingForClient() const;

    void OnRead();
    void OnProcess();
    void OnWrite();
    void Close();

    const std::shared_ptr<FileStream>& GetStream() const;
    int GetId() const;

    template <class T>
    void RecordProfileEvent() const;
    void RecordReadTimeoutEvent(Clock::TimePoint readyTime) const;
    void RecordWriteTimeoutEvent(Clock::TimePoint readyTime) const;

    bool FetchData(bool(Request::*)(std::size_t, std::size_t&), std::size_t maxRead);
    void ResetResponse();
    void LogNewRequest();
    void SendInternalError();
    bool FlushData(std::size_t maxWrite);

    std::weak_ptr<class Orchestrator> _orchestrator;
    std::uint64_t _id;
    std::shared_ptr<FileStream> _stream;
    Throttlers _throttlers;
    Request _request;
    Response _response;
    Synchronized<Clock::TimePoint> _timeout;
    std::atomic<Stage> _stage;
    std::mutex _setStageMutex;
    bool _forceClose = false;
    bool _fetchingContent = false;
    bool _autoFetchContent = true;
    Signal<> _readyToWrite;
    std::function<void()> _fetchContentCallback;

    friend class Orchestrator;
};

/**
 * Profiling
 */

class ChannelEvent : public ProfileEvent {
public:
    ChannelEvent(const char* source, std::uint64_t channelId);

    std::string GetSource() const override;
    std::string GetSummary() const override;
    void Accept(ProfileEventReader&) const override;

    std::uint64_t ChannelId;

private:
    const char* _source;
};

class ChannelActivating : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelActivated : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelReadable : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWritable : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelCompleted : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelTimeout : public ChannelEvent {
public:
    ChannelTimeout(const char* source,
                   std::uint64_t channelId,
                   Clock::TimePoint throttledTime,
                   Clock::TimePoint readyTime);

    Clock::TimePoint ThrottledTime;
    Clock::TimePoint ReadyTime;
};

class ChannelReadTimeout : public ChannelTimeout {
public:
    using ChannelTimeout::ChannelTimeout;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWriteTimeout : public ChannelTimeout {
public:
    using ChannelTimeout::ChannelTimeout;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWaitReadable : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWaitWritable : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelReading : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelRead : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWriting : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWritten : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelWrittenAll : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

class ChannelClosed : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

template <class T>
void Channel::RecordProfileEvent() const {
    Profiler::Record<T>("Channel", _id);
}

inline void Channel::RecordReadTimeoutEvent(Clock::TimePoint readyTime) const {
    Profiler::Record<ChannelReadTimeout>("Channel", _id, Clock::GetCurrentTime(), readyTime);
}

inline void Channel::RecordWriteTimeoutEvent(Clock::TimePoint readyTime) const {
    Profiler::Record<ChannelWriteTimeout>("Channel", _id, Clock::GetCurrentTime(), readyTime);
}

} // namespace Chili
