#pragma once

#include "Profiler.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <type_traits>

namespace Chili {

template <class T>
auto PtrShim(T t) {
    using U = std::decay_t<T>;
    using V = std::remove_pointer_t<U>;
    return std::shared_ptr<V>(t, [](U) {});
}

inline std::string GetEnvironmentVariable(const std::string& name) {
    auto result = std::getenv(name.c_str());
    return result ? result : "";
}

class AutoProfile {
public:
    AutoProfile() {
        _profile = !GetEnvironmentVariable("PROFILE").empty();

        if (_profile) {
            Profiler::Clear();
            Profiler::Enable();
        }
    }

    ~AutoProfile() {
        if (_profile) {
            std::cout << Profiler::GetProfile().GetSummary();
            Profiler::Disable();
            Profiler::Clear();
        }
    }

private:
    bool _profile;
};

} // namespace Chili
