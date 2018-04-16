#pragma once

#include "Clock.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Chili {

using Hz = std::uint64_t;

class ProfileEventReader {
public:
    void Visit(const class ProfileEvent&);

    virtual void Read(const class ProfileEvent&) {}
    virtual void Read(const class GenericProfileEvent&) {}

    /**
     * Channel events
     */

    virtual void Read(const class ChannelEvent&) {}
    virtual void Read(const class ChannelActivating&) {}
    virtual void Read(const class ChannelActivated&) {}
    virtual void Read(const class ChannelReadable&) {}
    virtual void Read(const class ChannelWritable&) {}
    virtual void Read(const class ChannelCompleted&) {}
    virtual void Read(const class ChannelReadTimeout&) {}
    virtual void Read(const class ChannelWriteTimeout&) {}
    virtual void Read(const class ChannelWaitReadable&) {}
    virtual void Read(const class ChannelWaitWritable&) {}
    virtual void Read(const class ChannelReading&) {}
    virtual void Read(const class ChannelWriting&) {}
    virtual void Read(const class ChannelWritten&) {}
    virtual void Read(const class ChannelClosed&) {}

    /**
     * Orchestrator events
     */

    virtual void Read(const class OrchestratorEvent&) {}
    virtual void Read(const class OrchestratorSignalled&) {}
    virtual void Read(const class OrchestratorWokeUp&) {}
    virtual void Read(const class OrchestratorWaiting&) {}
    virtual void Read(const class OrchestratorCapturingTasks&) {}

    /**
     * Poller events
     */

    virtual void Read(const class PollerEvent&) {}
    virtual void Read(const class PollerEventDispatched&) {}
    virtual void Read(const class PollerWokeUp&) {}
    virtual void Read(const class PollerWaiting&) {}

    /**
     * SocketServer events
     */

    virtual void Read(const class SocketServerEvent&) {}
    virtual void Read(const class SocketQueued&) {}
    virtual void Read(const class SocketDequeued&) {}
    virtual void Read(const class SocketAccepted&) {}
};

class ProfileEvent {
public:
    ProfileEvent();

    const Clock::TimePoint& GetTimePoint() const;

    virtual std::string GetSource() const = 0;
    virtual std::string GetSummary() const = 0;
    virtual void Accept(ProfileEventReader&) const = 0;

private:
    Clock::TimePoint _time_point;
};

class GenericProfileEvent : public ProfileEvent {
public:
    GenericProfileEvent(const char* source, std::shared_ptr<void> data);

    std::string GetSource() const override;
    std::string GetSummary() const override;
    void Accept(class ProfileEventReader& reader) const override;

    std::shared_ptr<const void> Data;

private:
    const char* _source;
};

class Profile {
public:
    std::string GetSummary() const;

    Clock::TimePoint GetStartTime() const;
    Clock::TimePoint GetEndTime() const;

    std::uint64_t GetTimesChannelsWereActivated() const;
    Hz GetRateChannelsWereActivated() const;
    Hz GetRateChannelsWereActivated(Clock::TimePoint) const;

    std::chrono::milliseconds GetChannelsUpTime() const;

    std::uint64_t GetTimesChannelsWaitedForReadability() const;
    Hz GetRateChannelsWaitedForReadability() const;
    Hz GetRateChannelsWaitedForReadability(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsWaitedForWritability() const;
    Hz GetRateChannelsWaitedForWritability() const;
    Hz GetRateChannelsWaitedForWritability(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsTimedOutOnReading() const;
    Hz GetRateChannelsTimedOutOnReading() const;
    Hz GetRateChannelsTimedOutOnReading(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsTimedOutOnWriting() const;
    Hz GetRateChannelsTimedOutOnWriting() const;
    Hz GetRateChannelsTimedOutOnWriting(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsBecameReadable() const;
    Hz GetRateChannelsBecameReadable() const;
    Hz GetRateChannelsBecameReadable(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsBecameWritable() const;
    Hz GetRateChannelsBecameWritable() const;
    Hz GetRateChannelsBecameWritable(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsWereReading() const;
    Hz GetRateChannelsWereReading() const;
    Hz GetRateChannelsWereReading(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsWereWriting() const;
    Hz GetRateChannelsWereWriting() const;
    Hz GetRateChannelsWereWriting(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsWroteFullResponse() const;
    Hz GetRateChannelsWroteFullResponse() const;
    Hz GetRateChannelsWroteFullResponse(Clock::TimePoint) const;

    std::uint64_t GetTimesChannelsWereClosed() const;
    Hz GetRateChannelsWereClosed() const;
    Hz GetRateChannelsWereClosed(Clock::TimePoint) const;

    std::uint64_t GetTimesOrchestratorWasSignalled() const;
    Hz GetRateOrchestratorWasSignalled() const;
    Hz GetRateOrchestratorWasSignalled(Clock::TimePoint) const;

    std::uint64_t GetTimesOrchestratorWokeUp() const;
    Hz GetRateOrchestratorWokeUp() const;
    Hz GetRateOrchestratorWokeUp(Clock::TimePoint) const;

    std::uint64_t GetTimesOrchestratorCapturedTasks() const;
    Hz GetRateOrchestratorCapturedTasks() const;
    Hz GetRateOrchestratorCapturedTasks(Clock::TimePoint) const;

    std::chrono::milliseconds GetOrchestratorUpTime() const;
    std::chrono::milliseconds GetOrchestratorIdleTime() const;

    std::uint64_t GetTimesPollerDispatchedAnEvent() const;
    Hz GetRatePollerDispatchedAnEvent() const;
    Hz GetRatePollerDispatchedAnEvent(Clock::TimePoint) const;

    std::chrono::milliseconds GetPollerUpTime() const;
    std::chrono::milliseconds GetPollerIdleTime() const;

    std::uint64_t GetTimesSocketQueued() const;
    Hz GetRateSocketQueued() const;
    Hz GetRateSocketQueued(Clock::TimePoint) const;

    std::uint64_t GetTimesSocketDequeued() const;
    Hz GetRateSocketDequeued() const;
    Hz GetRateSocketDequeued(Clock::TimePoint) const;

    std::uint64_t GetTimesSocketAccepted() const;
    Hz GetRateSocketAccepted() const;
    Hz GetRateSocketAccepted(Clock::TimePoint) const;

private:
    Profile();

    std::vector<std::reference_wrapper<const ProfileEvent>> _events;
    Clock::TimePoint _startTime;
    Clock::TimePoint _endTime;

    friend class Profiler;
};

class Profiler {
public:
    template <class T, class... Args>
    static void Record(Args... args);

    static void Enable();
    static void Disable();
    static void Clear();

    static std::vector<std::reference_wrapper<const ProfileEvent>> GetEvents();
    static Profile GetProfile();

private:
    static bool _enabled;
    static std::vector<std::unique_ptr<ProfileEvent>> _events;
    static Clock::TimePoint _startTime;
    static Clock::TimePoint _endTime;
    static std::mutex _mutex;

    friend class Profile;
};

template <class T, class... Args>
void Profiler::Record(Args... args) {
    std::lock_guard lock(_mutex);

    if (_enabled)
        _events.push_back(std::make_unique<T>(args...));
}

} // namespace Chili

