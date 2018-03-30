#pragma once

#include "Clock.h"
#include "Poller.h"
#include "Profiler.h"
#include "Request.h"
#include "Response.h"
#include "Signal.h"
#include "Synchronized.h"
#include "Throttler.h"

#include <mutex>

namespace Chili {

/**
 * ChannelBase
 */

class ChannelBase {
public:
    enum class Stage {
        WaitReadable,
        ReadTimeout,
        Read,
        Process,
        WaitWritable,
        WriteTimeout,
        Write,
        Closed
    };

    struct Throttlers {
        struct Group {
            Throttler Dedicated;
            std::shared_ptr<Throttler> Master;
        } Read, Write;
    };

    enum class Control {
        FetchContent,
        RejectContent,
        SendResponse
    };

    ChannelBase(std::shared_ptr<FileStream>);
    virtual ~ChannelBase();

protected:
    virtual Control Process() = 0;

private:
    struct ThrottlingInfo {
        std::size_t currentQuota;
        std::size_t capacity;
        bool full;
        Clock::TimePoint fillTime;
    };

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
    void LogNewRequest();
    void SendInternalError();
    void HandleControlDirective(Control);
    bool FlushData(std::size_t maxWrite);
    void SetRequestedTimeout(Clock::TimePoint);

    std::uint64_t _id;
    std::shared_ptr<FileStream> _stream;
    Throttlers _throttlers;
    Request _request;
    Response _response;
    Synchronized<Clock::TimePoint> _timeout;
    std::atomic<Stage> _stage;
    bool _forceClose = false;
    bool _fetchingContent = false;
    bool _autoFetchContent = true;

    friend class Channel;
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

class ChannelClosed : public ChannelEvent {
public:
    using ChannelEvent::ChannelEvent;
    void Accept(ProfileEventReader&) const override;
};

template <class T>
void ChannelBase::RecordProfileEvent() const {
    Profiler::Record<T>("ChannelBase", _id);
}

inline void ChannelBase::RecordReadTimeoutEvent(Clock::TimePoint readyTime) const {
    Profiler::Record<ChannelReadTimeout>("ChannelBase", _id, Clock::GetCurrentTime(), readyTime);
}

inline void ChannelBase::RecordWriteTimeoutEvent(Clock::TimePoint readyTime) const {
    Profiler::Record<ChannelWriteTimeout>("ChannelBase", _id, Clock::GetCurrentTime(), readyTime);
}

} // namespace Chili
