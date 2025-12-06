#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <type_traits>
#endif

#include "export.hpp"

POLLCORO_EXPORT namespace pollcoro {
    namespace detail {
    // Member operator co_await detection
    template<typename T, typename = void>
    struct has_member_co_await : std::false_type {};

    template<typename T>
    struct has_member_co_await<T, std::void_t<decltype(std::declval<T>().operator co_await())>>
        : std::true_type {};

    template<typename T>
    inline constexpr bool has_member_co_await_v = has_member_co_await<T>::value;

    // Free operator co_await detection
    template<typename T, typename = void>
    struct has_free_co_await : std::false_type {};

    template<typename T>
    struct has_free_co_await<T, std::void_t<decltype(operator co_await(std::declval<T>()))>>
        : std::true_type {};

    template<typename T>
    inline constexpr bool has_free_co_await_v = has_free_co_await<T>::value;

    // Awaiter type deduction
    template<typename T, typename = void>
    struct co_awaiter_impl {
        using type = T;
    };

    template<typename T>
    struct co_awaiter_impl<T, std::enable_if_t<has_member_co_await_v<T>>> {
        using type = decltype(std::declval<T>().operator co_await());
    };

    template<typename T>
    struct co_awaiter_impl<
        T,
        std::enable_if_t<!has_member_co_await_v<T> && has_free_co_await_v<T>>> {
        using type = decltype(operator co_await(std::declval<T>()));
    };

    template<typename T>
    using co_awaiter_t = typename co_awaiter_impl<T>::type;

    // Awaiter interface detection
    template<typename T, typename = void>
    struct has_co_await_ready : std::false_type {};

    template<typename T>
    struct has_co_await_ready<
        T,
        std::void_t<decltype(static_cast<bool>(std::declval<T>().await_ready()))>>
        : std::true_type {};

    template<typename T>
    inline constexpr bool has_co_await_ready_v = has_co_await_ready<T>::value;

    template<typename T, typename = void>
    struct has_co_await_resume : std::false_type {};

    template<typename T>
    struct has_co_await_resume<T, std::void_t<decltype(std::declval<T>().await_resume())>>
        : std::true_type {};

    template<typename T>
    inline constexpr bool has_co_await_resume_v = has_co_await_resume<T>::value;

    // Awaiter trait
    template<typename T>
    struct is_co_awaiter : std::bool_constant<has_co_await_ready_v<T> && has_co_await_resume_v<T>> {
    };

    template<typename T>
    inline constexpr bool is_co_awaiter_v = is_co_awaiter<T>::value;

    // Awaitable trait
    template<typename T, typename = void>
    struct is_co_awaitable : std::false_type {};

    template<typename T>
    struct is_co_awaitable<T, std::enable_if_t<is_co_awaiter_v<co_awaiter_t<T>>>> : std::true_type {
    };

    template<typename T>
    inline constexpr bool is_co_awaitable_v = is_co_awaitable<T>::value;

    // Await result type
    template<typename T, typename = void>
    struct co_await_result {};

    template<typename T>
    struct co_await_result<T, std::enable_if_t<is_co_awaitable_v<T>>> {
        using type = decltype(std::declval<co_awaiter_t<T>>().await_resume());
    };

    template<typename T>
    using co_await_result_t = typename co_await_result<T>::type;

    // Awaitable-of trait
    template<typename T, typename Result, typename = void>
    struct is_co_awaitable_of : std::false_type {};

    template<typename T, typename Result>
    struct is_co_awaitable_of<
        T,
        Result,
        std::enable_if_t<is_co_awaitable_v<T> && std::is_same_v<co_await_result_t<T>, Result>>>
        : std::true_type {};

    template<typename T, typename Result>
    inline constexpr bool is_co_awaitable_of_v = is_co_awaitable_of<T, Result>::value;

    // Scheduler trait
    template<typename T, typename = void>
    struct is_co_scheduler : std::false_type {};

    template<typename T>
    struct is_co_scheduler<
        T,
        std::enable_if_t<is_co_awaitable_v<decltype(std::declval<T>().schedule())>>>
        : std::true_type {};

    template<typename T>
    inline constexpr bool is_co_scheduler_v = is_co_scheduler<T>::value;
    }  // namespace detail

    // Public type aliases
    template<typename T>
    using co_awaiter_t = detail::co_awaiter_t<T>;

    template<typename T>
    using co_await_result_t = detail::co_await_result_t<T>;

    // Public traits
    template<typename T>
    using is_co_awaiter = detail::is_co_awaiter<T>;

    template<typename T>
    inline constexpr bool is_co_awaiter_v = is_co_awaiter<T>::value;

    template<typename T>
    using is_co_awaitable = detail::is_co_awaitable<T>;

    template<typename T>
    inline constexpr bool is_co_awaitable_v = is_co_awaitable<T>::value;

    template<typename T, typename Result>
    using is_co_awaitable_of = detail::is_co_awaitable_of<T, Result>;

    template<typename T, typename Result>
    inline constexpr bool is_co_awaitable_of_v = is_co_awaitable_of<T, Result>::value;

    template<typename T>
    using is_co_scheduler = detail::is_co_scheduler<T>;

    template<typename T>
    inline constexpr bool is_co_scheduler_v = is_co_scheduler<T>::value;

#if POLLCORO_USE_CONCEPTS
    template<typename T>
    concept co_awaiter = is_co_awaiter_v<T>;

    template<typename T>
    concept co_awaitable = is_co_awaitable_v<T>;

    template<typename T, typename Result>
    concept co_awaitable_of = is_co_awaitable_of_v<T, Result>;

    template<typename T>
    concept co_scheduler = is_co_scheduler_v<T>;
#endif

}  // namespace pollcoro