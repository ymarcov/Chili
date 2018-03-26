#include "Profiler.h"

namespace Nitra {

bool Profiler::_enabled = false;
std::vector<std::unique_ptr<ProfileEvent>> Profiler::_events;
std::mutex Profiler::_mutex;

void Profiler::Enable() {
    std::lock_guard<std::mutex> lock(_mutex);
    _enabled = true;
}

void Profiler::Disable() {
    std::lock_guard<std::mutex> lock(_mutex);
    _enabled = false;
}

void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _events.clear();
}

std::vector<std::reference_wrapper<const ProfileEvent>> Profiler::GetEvents() {
    std::lock_guard<std::mutex> lock(_mutex);
    auto result = std::vector<std::reference_wrapper<const ProfileEvent>>();

    result.reserve(_events.size());

    std::transform(begin(_events), end(_events),
                   std::back_inserter(result), [](auto& e) {
        return std::ref(const_cast<const ProfileEvent&>(*e));
    });

    return result;
}

void ProfileEventReader::Visit(const ProfileEvent& pe) {
    pe.Accept(*this);
}

ProfileEvent::ProfileEvent()
    : _time_point(Clock::GetCurrentTimePoint()) {}

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

} // namespace Nitra

