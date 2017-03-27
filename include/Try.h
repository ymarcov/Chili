#pragma once

#include <functional>

namespace Yam {
namespace Http {

class Try {
public:
    Try(std::function<void()> f) {
        _tryFunction = std::move(f);
    }

    void Finally(std::function<void()> f) && {
        _finallyFunction = std::move(f);
    }

    ~Try() {
        try {
            _tryFunction();
        } catch (...) {
            _finallyFunction();
            throw;
        }

        _finallyFunction();
    }

private:
    std::function<void()> _tryFunction;
    std::function<void()> _finallyFunction;
};

} // namespace Http
} // namespace Yam

