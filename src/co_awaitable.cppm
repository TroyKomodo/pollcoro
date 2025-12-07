module;

#include <concepts>
#include <type_traits>
#include <utility>

export module pollcoro:co_awaitable;

export namespace pollcoro {
namespace detail {
// Member operator co_await detection (lvalue)
template<typename T>
concept has_member_co_await_lvalue = requires(T t) {
    { t.operator co_await() };
};

// Member operator co_await detection (rvalue)
template<typename T>
concept has_member_co_await_rvalue = requires(T t) {
    { std::move(t).operator co_await() };
};

// Combined member operator co_await detection
template<typename T>
concept has_member_co_await = has_member_co_await_lvalue<T> || has_member_co_await_rvalue<T>;

// Free operator co_await detection
template<typename T>
concept has_free_co_await = requires(T t) {
    { operator co_await(t) };
};

// Awaiter type deduction - base case (type is its own awaiter)
template<typename T, typename = void>
struct co_awaiter_impl {
    using type = T;
};

// Awaiter type deduction - lvalue member operator co_await
template<typename T>
struct co_awaiter_impl<T, std::enable_if_t<has_member_co_await_lvalue<T>>> {
    using type = decltype(std::declval<T&>().operator co_await());
};

// Awaiter type deduction - rvalue member operator co_await (but not lvalue)
template<typename T>
struct co_awaiter_impl<
    T,
    std::enable_if_t<!has_member_co_await_lvalue<T> && has_member_co_await_rvalue<T>>> {
    using type = decltype(std::declval<T&&>().operator co_await());
};

// Awaiter type deduction - free operator co_await (but no member)
template<typename T>
struct co_awaiter_impl<T, std::enable_if_t<!has_member_co_await<T> && has_free_co_await<T>>> {
    using type = decltype(operator co_await(std::declval<T>()));
};

template<typename T>
using co_awaiter_t = typename co_awaiter_impl<T>::type;

// Awaiter interface detection
template<typename T>
concept has_co_await_ready = requires(T t) {
    { static_cast<bool>(t.await_ready()) };
};

template<typename T>
concept has_co_await_resume = requires(T t) {
    { t.await_resume() };
};

// Awaiter concept
template<typename T>
concept co_awaiter_impl_concept = has_co_await_ready<T> && has_co_await_resume<T>;

// Awaitable concept
template<typename T>
concept co_awaitable_impl_concept = co_awaiter_impl_concept<co_awaiter_t<T>>;

// Await result type
template<typename T>
struct co_await_result {};

template<co_awaitable_impl_concept T>
struct co_await_result<T> {
    using type = std::decay_t<decltype(std::declval<co_awaiter_t<T>>().await_resume())>;
};

template<typename T>
using co_await_result_impl_t = typename co_await_result<T>::type;

// Awaitable-of concept
template<typename T, typename Result>
concept co_awaitable_of_impl =
    co_awaitable_impl_concept<T> && std::same_as<co_await_result_impl_t<T>, Result>;

// Scheduler concept
template<typename T>
concept co_scheduler_impl = requires(T t) {
    { t.schedule() } -> co_awaitable_impl_concept;
};
}  // namespace detail

// Public type aliases
template<typename T>
using co_awaiter_t = detail::co_awaiter_t<T>;

template<typename T>
using co_await_result_t = detail::co_await_result_impl_t<T>;

// Public concepts
template<typename T>
concept co_awaiter = detail::co_awaiter_impl_concept<T>;

template<typename T>
concept co_awaitable = detail::co_awaitable_impl_concept<T>;

template<typename T, typename Result>
concept co_awaitable_of = detail::co_awaitable_of_impl<T, Result>;

template<typename T>
concept co_scheduler = detail::co_scheduler_impl<T>;

// Legacy traits for backwards compatibility
template<typename T>
using is_co_awaiter = std::bool_constant<co_awaiter<T>>;

template<typename T>
inline constexpr bool is_co_awaiter_v = co_awaiter<T>;

template<typename T>
using is_co_awaitable = std::bool_constant<co_awaitable<T>>;

template<typename T>
inline constexpr bool is_co_awaitable_v = co_awaitable<T>;

template<typename T, typename Result>
using is_co_awaitable_of = std::bool_constant<co_awaitable_of<T, Result>>;

template<typename T, typename Result>
inline constexpr bool is_co_awaitable_of_v = co_awaitable_of<T, Result>;

template<typename T>
using is_co_scheduler = std::bool_constant<co_scheduler<T>>;

template<typename T>
inline constexpr bool is_co_scheduler_v = co_scheduler<T>;

}  // namespace pollcoro