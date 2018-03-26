#pragma once

#include "Clock.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Nitra {

class ProfileEventReader {
public:
    void Visit(const class ProfileEvent&);

    virtual void Read(const class ProfileEvent&) {}
    virtual void Read(const class GenericProfileEvent&) {}

    /**
     * Channel events
     */
    virtual void Read(const class ChannelEvent&) {}
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

class Profiler {
public:
    template <class T, class... Args>
    static void Record(Args... args);

    static void Enable();
    static void Disable();
    static void Clear();
    static std::vector<std::reference_wrapper<const ProfileEvent>> GetEvents();

private:
    static bool _enabled;
    static std::vector<std::unique_ptr<ProfileEvent>> _events;
    static std::mutex _mutex;
};

template <class T, class... Args>
void Profiler::Record(Args... args) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_enabled)
        _events.push_back(std::make_unique<T>(args...));
}

} // namespace Nitra

