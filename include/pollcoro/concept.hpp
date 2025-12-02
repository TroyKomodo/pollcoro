#pragma once

#include "export.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {

namespace detail {
template<typename T>
inline constexpr bool is_pollable_state_v = false;

template<typename T>
inline constexpr bool is_pollable_state_v<pollable_state<T>> = true;

template<typename T>
struct pollable_state_traits;

template<typename T>
struct pollable_state_traits<pollable_state<T>> {
    using result_type = T;
};

template<typename T, typename = void>
struct awaitable_traits : std::false_type {};

template<typename T>
struct awaitable_traits<T, std::void_t<decltype(std::declval<T>().on_poll(std::declval<waker&>()))>>
    : std::bool_constant<
          is_pollable_state_v<decltype(std::declval<T>().on_poll(std::declval<waker&>()))>> {};

template<typename... Ts>
constexpr bool awaitable_v = (awaitable_traits<Ts>::value && ...);
}  // namespace detail

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L

#define POLLCORO_CONCEPT(name) name

template<typename T>
concept awaitable = requires(T t, waker& w) { t.on_poll(w); } && detail::awaitable_v<T>;
#else

#define POLLCORO_CONCEPT(name) typename

#endif

#define POLLCORO_STATIC_ASSERT(awaitable)                           \
    static_assert(                                                  \
        pollcoro::detail::awaitable_v<awaitable>,                   \
        "The awaitable type does not satisfy the awaitable concept" \
    )

template<POLLCORO_CONCEPT(awaitable) T>
using awaitable_result_t = typename detail::pollable_state_traits<
    decltype(std::declval<T>().on_poll(std::declval<waker&>()))>::result_type;

}  // namespace pollcoro
