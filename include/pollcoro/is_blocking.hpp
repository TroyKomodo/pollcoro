#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <type_traits>
#endif

namespace pollcoro {
namespace detail {
template<typename T, typename = void>
struct awaitable_is_blocking {
    static constexpr bool value = true;
};

template<typename T>
struct awaitable_is_blocking<T, std::void_t<decltype(T::is_blocking_v)>> {
    static constexpr bool value = T::is_blocking_v;
};

template<typename... Ts>
constexpr bool awaitable_is_blocking_v = (awaitable_is_blocking<Ts>::value || ...);
}  // namespace detail

struct awaitable_always_blocks {
    static constexpr bool is_blocking_v = true;
};

struct awaitable_never_blocks {
    static constexpr bool is_blocking_v = false;
};

template<typename... Ts>
struct awaitable_maybe_blocks {
    static constexpr bool is_blocking_v = (detail::awaitable_is_blocking_v<Ts> || ...);
};

template<typename T>
constexpr bool is_blocking_v = detail::awaitable_is_blocking_v<T>;
}  // namespace pollcoro
