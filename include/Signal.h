#pragma once

#include <functional>
#include <vector>

namespace Yam {
namespace Http {

template <class... Param>
class Signal {
public:
    using Callback = std::function<void(Param...)>;

    void operator()(Param&&... p) const {
        for (auto& s : _subscribers)
            s(std::forward<Param>(p)...);
    }

    template <class T>
    Signal& operator+=(T&& f) {
        _subscribers.push_back(std::forward<T>(f));
        return *this;
    }

private:
    std::vector<Callback> _subscribers;
};

} // namespace Http
} // namespace Yam

