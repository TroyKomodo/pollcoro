module;

#include <concepts>

export module pollcoro:is_blocking;

export namespace pollcoro {
namespace detail {
template<typename T>
concept has_is_blocking = requires {
    { T::is_blocking_v } -> std::convertible_to<bool>;
};

template<typename T>
constexpr bool awaitable_is_blocking_impl() {
    if constexpr (has_is_blocking<T>) {
        return T::is_blocking_v;
    } else {
        return true;
    }
}

template<typename... Ts>
constexpr bool awaitable_is_blocking_v = (awaitable_is_blocking_impl<Ts>() || ...);
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
constexpr bool is_blocking_v = detail::awaitable_is_blocking_impl<T>();
}  // namespace pollcoro
