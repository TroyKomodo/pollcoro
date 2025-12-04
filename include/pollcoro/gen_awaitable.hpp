#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <optional>
#endif

#include "concepts.hpp"
#include "export.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class gen_awaitable_state {
        constexpr explicit gen_awaitable_state(T result) : result_(std::move(result)) {}

        explicit gen_awaitable_state(bool done) : done_(done) {}

      public:
        using result_type = T;
        gen_awaitable_state() = default;

        static constexpr gen_awaitable_state ready(T result) {
            return gen_awaitable_state(std::move(result));
        }

        static constexpr gen_awaitable_state pending() {
            return gen_awaitable_state(false);
        }

        static constexpr gen_awaitable_state done() {
            return gen_awaitable_state(true);
        }

        bool is_ready() const {
            return result_.has_value();
        }

        bool is_done() const {
            return done_;
        }

        T take_result() {
            return std::move(*result_);
        }

        template<typename Func>
        auto map(Func&& func) && {
            using U = std::invoke_result_t<Func, T>;
            if (is_done()) {
                return gen_awaitable_state<U>::done();
            }
            if (!is_ready()) {
                return gen_awaitable_state<U>::pending();
            }

            return gen_awaitable_state<U>::ready(func(take_result()));
        }

      private:
        std::optional<T> result_ = std::nullopt;
        bool done_{false};
    };

    namespace detail {
    template<typename T>
    inline constexpr bool is_gen_awaitable_state_v = false;

    template<typename T>
    inline constexpr bool is_gen_awaitable_state_v<gen_awaitable_state<T>> = true;

    template<typename T>
    struct gen_awaitable_state_traits;

    template<typename T>
    struct gen_awaitable_state_traits<gen_awaitable_state<T>> {
        using result_type = T;
    };

    template<typename T, typename = void>
    struct gen_awaitable_traits : std::false_type {};

    template<typename T>
    struct gen_awaitable_traits<
        T,
        std::void_t<decltype(std::declval<T>().poll_next(std::declval<const waker&>()))>>
        : std::bool_constant<is_gen_awaitable_state_v<
              decltype(std::declval<T>().poll_next(std::declval<const waker&>()))>> {};

    template<typename... Ts>
    constexpr bool gen_awaitable_v = (gen_awaitable_traits<Ts>::value && ...);

    }  // namespace detail

#if POLLCORO_USE_CONCEPTS
    template<typename T>
    concept gen_awaitable =
        requires(T t, const waker& w) { t.poll_next(w); } && detail::gen_awaitable_v<T>;
#endif

    template<POLLCORO_CONCEPT(gen_awaitable) T>
    using gen_awaitable_result_t = typename detail::gen_awaitable_state_traits<
        decltype(std::declval<T>().poll_next(std::declval<const waker&>()))>::result_type;

}  // namespace pollcoro

#define POLLCORO_STATIC_ASSERT_GEN(gen_awaitable)                           \
    static_assert(                                                          \
        pollcoro::detail::gen_awaitable_v<gen_awaitable>,                   \
        "The gen_awaitable type does not satisfy the gen_awaitable concept" \
    )
