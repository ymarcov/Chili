#include "Profiler.h"
#include "Channel.h"
#include "Orchestrator.h"
#include "Poller.h"
#include "Acceptor.h"

#include <chrono>
#include <cstdlib>
#include <fmt/format.h>
#include <map>

using namespace std::literals;

namespace Chili {

std::atomic_bool Profiler::_enabled = false;
std::vector<std::unique_ptr<ProfileEvent>> Profiler::_events;
Clock::TimePoint Profiler::_startTime{Clock::GetCurrentTime()};
Clock::TimePoint Profiler::_endTime{Clock::GetCurrentTime()};
std::mutex Profiler::_mutex;

Profile::Profile()
    : _events(Profiler::GetEvents())
    , _startTime(Profiler::_startTime)
    , _endTime(Profiler::_endTime) {
    if (_endTime < _startTime)
        _endTime = Clock::GetCurrentTime();
}

std::string Profile::GetSummary() const {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(GetEndTime() - GetStartTime());

    auto seconds = double(duration.count()) / 1000;

    auto w = fmt::MemoryWriter();

    w.write("Profile Summary\n");
    w.write("===============\n");

    w.write("[General] Duration: {} seconds\n", seconds);

    w.write("[Channel] # Activated: {} ({}/sec)\n",
            GetTimesChannelsWereActivated(),
            GetRateChannelsWereActivated());

    w.write("[Channel] # Closed: {} ({}/sec)\n",
            GetTimesChannelsWereClosed(),
            GetRateChannelsWereClosed());

    w.write("[Channel] Up Time: {} ms\n", GetChannelsUpTime().count());

    w.write("[Channel::Read] # Waited for Readability: {} ({}/sec)\n",
            GetTimesChannelsWaitedForReadability(),
            GetRateChannelsWaitedForReadability());

    w.write("[Channel::Read] # Became Readable: {} ({}/sec)\n",
            GetTimesChannelsBecameReadable(),
            GetRateChannelsBecameReadable());

    w.write("[Channel::Read] # Timed Out on Reading: {} ({}/sec)\n",
            GetTimesChannelsTimedOutOnReading(),
            GetRateChannelsTimedOutOnReading());

    w.write("[Channel::Read] # Reading: {} ({}/sec)\n",
            GetTimesChannelsWereReading(),
            GetRateChannelsWereReading());

    w.write("[Channel::Read] Total Time: {} ms\n", GetTimeSpentWhileChannelsWereReading().count());

    w.write("[Channel::Write] # Waited for Writability: {} ({}/sec)\n",
            GetTimesChannelsWaitedForWritability(),
            GetRateChannelsWaitedForWritability());

    w.write("[Channel::Write] # Became Writable: {} ({}/sec)\n",
            GetTimesChannelsBecameWritable(),
            GetRateChannelsBecameWritable());

    w.write("[Channel::Write] # Timed Out on Writing: {} ({}/sec)\n",
            GetTimesChannelsTimedOutOnWriting(),
            GetRateChannelsTimedOutOnWriting());

    w.write("[Channel::Write] # Writing: {} ({}/sec)\n",
            GetTimesChannelsWereWriting(),
            GetRateChannelsWereWriting());

    w.write("[Channel::Write] # Wrote Full Response: {} ({}/sec)\n",
            GetTimesChannelsWroteFullResponse(),
            GetRateChannelsWroteFullResponse());

    w.write("[Channel::Write] Total Time: {} ms\n", GetTimeSpentWhileChannelsWereWriting().count());

    w.write("[Orchestrator] # Signalled: {} ({}/sec)\n",
            GetTimesOrchestratorWasSignalled(),
            GetRateOrchestratorWasSignalled());

    w.write("[Orchestrator] # Woke Up: {} ({}/sec)\n",
            GetTimesOrchestratorWokeUp(),
            GetRateOrchestratorWokeUp());

    w.write("[Orchestrator] # Times Captured Tasks: {} ({}/sec)\n",
            GetTimesOrchestratorCapturedTasks(),
            GetRateOrchestratorCapturedTasks());

    w.write("[Orchestrator] Up Time: {} ms\n", GetOrchestratorUpTime().count());

    w.write("[Orchestrator] Idle Time: {} ms\n", GetOrchestratorIdleTime().count());

    w.write("[Poller] # Events Dispatched: {} ({}/sec)\n",
            GetTimesPollerDispatchedAnEvent(),
            GetRatePollerDispatchedAnEvent());

    w.write("[Poller] Up Time: {} ms\n", GetPollerUpTime().count());

    w.write("[Poller] Idle Time: {} ms\n", GetPollerIdleTime().count());

    w.write("[Acceptor] # Sockets Queued: {} ({}/sec)\n",
            GetTimesSocketQueued(),
            GetRateSocketQueued());

    w.write("[Acceptor] # Sockets Dequeued: {} ({}/sec)\n",
            GetTimesSocketDequeued(),
            GetRateSocketDequeued());

    w.write("[Acceptor] # Sockets Accepted: {} ({}/sec)\n",
            GetTimesSocketAccepted(),
            GetRateSocketAccepted());

    return w.str();
}

Clock::TimePoint Profile::GetStartTime() const {
    return _startTime;
}

Clock::TimePoint Profile::GetEndTime() const {
    return _endTime;
}

namespace {

template <class T>
class Counter : public ProfileEventReader {
public:
    void Read(const T&) override {
        ++Count;
    }

    template <class Events>
    std::uint64_t ReadAll(const Events& events) {
        for (auto& e : events)
            Visit(e);

        return Count;
    }

    std::uint64_t Count;
};

template <class T>
class HzCalculator : public ProfileEventReader {
public:
    HzCalculator()
        : _accountForAll(true) {}

    HzCalculator(Clock::TimePoint position)
        : _position(position) {}

    void Read(const T& e) override {
        if (_accountForAll) {
            _window.push_back(&e);
        } else {
            auto eTime = e.GetTimePoint();

            if ((eTime > _position) && (eTime < _position + 1s))
                _window.push_back(&e);
        }
    }

    template <class Events>
    Hz ReadAll(const Events& events) {
        for (auto& e : events)
            Visit(e);

        if (!_accountForAll) {
            return _window.size();
        } else {
            if (_window.empty())
                return 0;

            auto start = _window.front()->GetTimePoint();
            auto end = _window.back()->GetTimePoint();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            return _window.size() / (double(elapsedMs) / 1000);
        }
    }

    bool _accountForAll = false;
    Clock::TimePoint _position;
    std::vector<const T*> _window;
};

template <class Begin, class End>
class ActivityCalculator {
public:
    template <class Events>
    std::chrono::milliseconds GetUpTime(Events& events) const {
        struct : ProfileEventReader {
            void Read(const Begin& e) {
                if (!_hasWakeUpData)
                    return;

                UpTime += std::chrono::duration_cast<std::chrono::milliseconds>(e.GetTimePoint() - _lastWakeUp);
            }

            void Read(const End& e) {
                _lastWakeUp = e.GetTimePoint();
                _hasWakeUpData = true;
            }

            bool _hasWakeUpData = false;
            Clock::TimePoint _lastWakeUp;
            std::chrono::milliseconds UpTime{0};
        } reader;

        for (auto& e : events)
            reader.Visit(e);

        return reader.UpTime;
    }

    template <class Events>
    std::chrono::milliseconds GetIdleTime(Events& events) const {
        struct : ProfileEventReader {
            void Read(const Begin& e) {
                _lastWait = e.GetTimePoint();
                _hasFirstWait = true;
            }

            void Read(const End& e) {
                if (!_hasFirstWait)
                    return;

                IdleTime += std::chrono::duration_cast<std::chrono::microseconds>(e.GetTimePoint() - _lastWait);
            }

            Clock::TimePoint _lastWait;
            std::chrono::microseconds IdleTime{0};
            bool _hasFirstWait = false;
        } reader;

        for (auto& e : events)
            reader.Visit(e);

        return std::chrono::duration_cast<std::chrono::milliseconds>(reader.IdleTime);
    }
};

} // unnamed namespace

std::uint64_t Profile::GetTimesChannelsWereActivated() const {
    return Counter<ChannelActivated>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereActivated() const {
    return HzCalculator<ChannelActivated>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereActivated(Clock::TimePoint t) const {
    return HzCalculator<ChannelActivated>(t).ReadAll(_events);
}

std::chrono::milliseconds Profile::GetChannelsUpTime() const {
    struct : ProfileEventReader {
        void Read(const ChannelActivating& e) {
            _lastActivationTime[e.ChannelId] = e.GetTimePoint();
        }

        void Read(const ChannelActivated& e) {
            UpTime += std::chrono::duration_cast<std::chrono::milliseconds>(e.GetTimePoint() - _lastActivationTime[e.ChannelId]);
        }

        std::chrono::milliseconds UpTime{0};
        std::map<std::uint64_t, Clock::TimePoint> _lastActivationTime;
    } reader;

    for (auto& e : _events)
        reader.Visit(e);

    return reader.UpTime;
}

std::uint64_t Profile::GetTimesChannelsWaitedForReadability() const {
    return Counter<ChannelWaitReadable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWaitedForReadability() const {
    return HzCalculator<ChannelWaitReadable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWaitedForReadability(Clock::TimePoint t) const {
    return HzCalculator<ChannelWaitReadable>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsWaitedForWritability() const {
    return Counter<ChannelWaitWritable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWaitedForWritability() const {
    return HzCalculator<ChannelWaitWritable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWaitedForWritability(Clock::TimePoint t) const {
    return HzCalculator<ChannelWaitWritable>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsTimedOutOnReading() const {
    return Counter<ChannelReadTimeout>().ReadAll(_events);
}

Hz Profile::GetRateChannelsTimedOutOnReading() const {
    return HzCalculator<ChannelReadTimeout>().ReadAll(_events);
}

Hz Profile::GetRateChannelsTimedOutOnReading(Clock::TimePoint t) const {
    return HzCalculator<ChannelReadTimeout>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsTimedOutOnWriting() const {
    return Counter<ChannelWriteTimeout>().ReadAll(_events);
}

Hz Profile::GetRateChannelsTimedOutOnWriting() const {
    return HzCalculator<ChannelWriteTimeout>().ReadAll(_events);
}

Hz Profile::GetRateChannelsTimedOutOnWriting(Clock::TimePoint t) const {
    return HzCalculator<ChannelWriteTimeout>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsBecameReadable() const {
    return Counter<ChannelReadable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsBecameReadable() const {
    return HzCalculator<ChannelReadable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsBecameReadable(Clock::TimePoint t) const {
    return HzCalculator<ChannelReadable>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsBecameWritable() const {
    return Counter<ChannelWritable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsBecameWritable() const {
    return HzCalculator<ChannelWritable>().ReadAll(_events);
}

Hz Profile::GetRateChannelsBecameWritable(Clock::TimePoint t) const {
    return HzCalculator<ChannelWritable>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsWereReading() const {
    return Counter<ChannelReading>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereReading() const {
    return HzCalculator<ChannelReading>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereReading(Clock::TimePoint t) const {
    return HzCalculator<ChannelReading>(t).ReadAll(_events);
}

std::chrono::milliseconds Profile::GetTimeSpentWhileChannelsWereReading() const {
    return ActivityCalculator<ChannelReading, ChannelRead>().GetUpTime(_events);
}

std::uint64_t Profile::GetTimesChannelsWereWriting() const {
    return Counter<ChannelWriting>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereWriting() const {
    return HzCalculator<ChannelWriting>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereWriting(Clock::TimePoint t) const {
    return HzCalculator<ChannelWriting>(t).ReadAll(_events);
}

std::chrono::milliseconds Profile::GetTimeSpentWhileChannelsWereWriting() const {
    return ActivityCalculator<ChannelWriting, ChannelWritten>().GetUpTime(_events);
}

std::uint64_t Profile::GetTimesChannelsWroteFullResponse() const {
    return Counter<ChannelWrittenAll>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWroteFullResponse() const {
    return HzCalculator<ChannelWrittenAll>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWroteFullResponse(Clock::TimePoint t) const {
    return HzCalculator<ChannelWrittenAll>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesChannelsWereClosed() const {
    return Counter<ChannelClosed>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereClosed() const {
    return HzCalculator<ChannelClosed>().ReadAll(_events);
}

Hz Profile::GetRateChannelsWereClosed(Clock::TimePoint t) const {
    return HzCalculator<ChannelClosed>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesOrchestratorWasSignalled() const {
    return Counter<OrchestratorSignalled>().ReadAll(_events);
}

Hz Profile::GetRateOrchestratorWasSignalled() const {
    return HzCalculator<OrchestratorSignalled>().ReadAll(_events);
}

Hz Profile::GetRateOrchestratorWasSignalled(Clock::TimePoint t) const {
    return HzCalculator<OrchestratorSignalled>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesOrchestratorWokeUp() const {
    return Counter<OrchestratorWokeUp>().ReadAll(_events);
}

Hz Profile::GetRateOrchestratorWokeUp() const {
    return HzCalculator<OrchestratorWokeUp>().ReadAll(_events);
}

Hz Profile::GetRateOrchestratorWokeUp(Clock::TimePoint t) const {
    return HzCalculator<OrchestratorWokeUp>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesOrchestratorCapturedTasks() const {
    return Counter<OrchestratorCapturingTasks>().ReadAll(_events);
}

Hz Profile::GetRateOrchestratorCapturedTasks() const {
    return HzCalculator<OrchestratorCapturingTasks>().ReadAll(_events);
}

Hz Profile::GetRateOrchestratorCapturedTasks(Clock::TimePoint t) const {
    return HzCalculator<OrchestratorCapturingTasks>(t).ReadAll(_events);
}

std::chrono::milliseconds Profile::GetOrchestratorUpTime() const {
    return ActivityCalculator<OrchestratorWaiting, OrchestratorWokeUp>().GetUpTime(_events);
}

std::chrono::milliseconds Profile::GetOrchestratorIdleTime() const {
    return ActivityCalculator<OrchestratorWaiting, OrchestratorWokeUp>().GetIdleTime(_events);
}

std::uint64_t Profile::GetTimesPollerDispatchedAnEvent() const {
    return Counter<PollerEventDispatched>().ReadAll(_events);
}

Hz Profile::GetRatePollerDispatchedAnEvent() const {
    return HzCalculator<PollerEventDispatched>().ReadAll(_events);
}

Hz Profile::GetRatePollerDispatchedAnEvent(Clock::TimePoint t) const {
    return HzCalculator<PollerEventDispatched>(t).ReadAll(_events);
}

std::chrono::milliseconds Profile::GetPollerUpTime() const {
    return ActivityCalculator<PollerWaiting, PollerWokeUp>().GetUpTime(_events);
}

std::chrono::milliseconds Profile::GetPollerIdleTime() const {
    return ActivityCalculator<PollerWaiting, PollerWokeUp>().GetIdleTime(_events);
}

std::uint64_t Profile::GetTimesSocketQueued() const {
    return Counter<SocketQueued>().ReadAll(_events);
}

Hz Profile::GetRateSocketQueued() const {
    return HzCalculator<SocketQueued>().ReadAll(_events);
}

Hz Profile::GetRateSocketQueued(Clock::TimePoint t) const {
    return HzCalculator<SocketQueued>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesSocketDequeued() const {
    return Counter<SocketDequeued>().ReadAll(_events);
}

Hz Profile::GetRateSocketDequeued() const {
    return HzCalculator<SocketDequeued>().ReadAll(_events);
}

Hz Profile::GetRateSocketDequeued(Clock::TimePoint t) const {
    return HzCalculator<SocketDequeued>(t).ReadAll(_events);
}

std::uint64_t Profile::GetTimesSocketAccepted() const {
    return Counter<SocketAccepted>().ReadAll(_events);
}

Hz Profile::GetRateSocketAccepted() const {
    return HzCalculator<SocketAccepted>().ReadAll(_events);
}

Hz Profile::GetRateSocketAccepted(Clock::TimePoint t) const {
    return HzCalculator<SocketAccepted>(t).ReadAll(_events);
}

void Profiler::Enable() {
    std::lock_guard lock(_mutex);
    _startTime = Clock::GetCurrentTime();
    _enabled = true;
}

void Profiler::Disable() {
    std::lock_guard lock(_mutex);
    _endTime = Clock::GetCurrentTime();
    _enabled = false;
}

void Profiler::Clear() {
    std::lock_guard lock(_mutex);
    _events.clear();
}

std::vector<std::reference_wrapper<const ProfileEvent>> Profiler::GetEvents() {
    std::lock_guard lock(_mutex);
    auto result = std::vector<std::reference_wrapper<const ProfileEvent>>();

    result.reserve(_events.size());

    std::transform(begin(_events), end(_events),
                   std::back_inserter(result), [](auto& e) {
        return std::ref(const_cast<const ProfileEvent&>(*e));
    });

    return result;
}

Profile Profiler::GetProfile() {
    return Profile();
}

void ProfileEventReader::Visit(const ProfileEvent& pe) {
    pe.Accept(*this);
}

ProfileEvent::ProfileEvent()
    : _time_point(Clock::GetCurrentTime()) {}

const Clock::TimePoint& ProfileEvent::GetTimePoint() const {
    return _time_point;
}

GenericProfileEvent::GenericProfileEvent(const char* source, std::shared_ptr<void> data = {})
    : Data(std::move(data))
    , _source(source) {}

std::string GenericProfileEvent::GetSource() const {
    return _source;
}

std::string GenericProfileEvent::GetSummary() const {
    if (Data)
        return "GenericProfileEvent (no data)";
    else
        return "GenericProfileEvent (has data)";
}

void GenericProfileEvent::Accept(class ProfileEventReader& reader) const {
    reader.Read(*this);
}

} // namespace Chili

