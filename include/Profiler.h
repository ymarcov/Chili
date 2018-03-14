#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Nitra {

class ProfileEventReader {
public:
    void Visit(const class ProfileEvent&);

    virtual void Read(const class ProfileEvent&) {}
    virtual void Read(const class GenericProfileEvent&) {}
};

class ProfileEvent {
public:
    ProfileEvent();

    const std::chrono::steady_clock::time_point& GetTimePoint() const;

    virtual std::string GetSource() const = 0;
    virtual std::string GetSummary() const = 0;
    virtual void Accept(ProfileEventReader&) const = 0;

private:
    std::chrono::steady_clock::time_point _time_point;
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
    static void Record(Args... args) {
        _events.push_back(std::make_unique<T>(args...));
    }

    static std::vector<std::reference_wrapper<const ProfileEvent>> GetEvents() {
        auto result = std::vector<std::reference_wrapper<const ProfileEvent>>();

        result.reserve(_events.size());

        std::transform(begin(_events), end(_events),
                       std::back_inserter(result), [](auto& e) {
            return std::ref(const_cast<const ProfileEvent&>(*e));
        });

        return result;
    }

private:
    static std::vector<std::unique_ptr<ProfileEvent>> _events;
};

} // namespace Nitra

