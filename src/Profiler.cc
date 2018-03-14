#include "Profiler.h"

namespace Nitra {

void ProfileEventReader::Visit(const ProfileEvent& pe) {
    pe.Accept(*this);
}

ProfileEvent::ProfileEvent()
    : _time_point(std::chrono::steady_clock::now()) {}

const std::chrono::steady_clock::time_point& ProfileEvent::GetTimePoint() const {
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

