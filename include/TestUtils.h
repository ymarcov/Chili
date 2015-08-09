#pragma once

#include <memory>
#include <type_traits>

namespace Yam {
namespace Http {

template <class T>
auto PtrShim(T t) {
    using U = std::decay_t<T>;
    using V = std::remove_pointer_t<U>;
    return std::shared_ptr<V>(t, [](U) {});
}

} // namespace Http
} // namespace Yam
