#pragma once

namespace Yam {
namespace Http {

template <class Callable>
class ExitTrap {
public:
    ExitTrap(Callable c) :
        _c(std::move(c)) {}

    ~ExitTrap() {
        _c();
    }

private:
    Callable _c;
};

template <class Callable>
ExitTrap<Callable> CreateExitTrap(Callable c) {
    return ExitTrap<Callable>(std::move(c));
}

} // namespace Http
} // namespace Yam

