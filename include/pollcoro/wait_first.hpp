#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <algorithm>
#include <iterator>
#include <map>
#include <tuple>
#include <utility>
#include <vector>
#endif

#include "concept.hpp"
#include "export.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(awaitable)... Awaitables>
    class wait_first_awaitable {
        POLLCORO_STATIC_ASSERT(Awaitables...);

        static_assert(sizeof...(Awaitables) > 0, "wait_first requires at least one awaitable");

        // All awaitables must have the same result type for wait_first
        using first_result_t =
            awaitable_result_t<std::tuple_element_t<0, std::tuple<Awaitables...>>>;

      public:
        using result_type = std::tuple<first_result_t, size_t>;

        explicit wait_first_awaitable(Awaitables... awaitables)
            : awaitables_(std::move(awaitables)...) {}

        pollable_state<result_type> on_poll(const waker& w) {
            return get_first_ready_result(std::index_sequence_for<Awaitables...>{}, w);
        }

      private:
        template<size_t I>
        auto try_get_result(const waker& w) {
            return std::get<I>(awaitables_).on_poll(w).map([](auto result) {
                return std::make_tuple(std::move(result), I);
            });
        }

        template<size_t... Is>
        auto get_first_ready_result(std::index_sequence<Is...>, const waker& w) {
            pollable_state<result_type> result = pollable_state<result_type>::pending();
            ((result = try_get_result<Is>(w), result.is_ready()) || ...);
            return std::move(result);
        }

        std::tuple<Awaitables...> awaitables_;
        bool completed_{false};
    };

    template<
        typename VecType,
        POLLCORO_CONCEPT(awaitable) Awaitable = decltype(*std::begin(std::declval<VecType&>()))>
    class wait_first_iter_awaitable {
        POLLCORO_STATIC_ASSERT(Awaitable);

        using result_t = awaitable_result_t<Awaitable>;

      public:
        using result_type = std::tuple<result_t, size_t>;

        explicit wait_first_iter_awaitable(VecType& awaitables) : awaitables_(awaitables) {}

        pollable_state<result_type> on_poll(const waker& w) {
            size_t i = 0;
            for (auto& awaitable : awaitables_) {
                auto state = awaitable.on_poll(w);
                if (state.is_ready()) {
                    if constexpr (std::is_void_v<result_t>) {
                        state.get_result();
                        return pollable_state<result_type>::ready(std::make_tuple(result_t{}, i));
                    } else {
                        return pollable_state<result_type>::ready(
                            std::make_tuple(state.take_result(), i)
                        );
                    }
                }
                i++;
            }
            return pollable_state<result_type>::pending();
        }

      private:
        VecType& awaitables_;
    };

    template<
        typename VecType,
        POLLCORO_CONCEPT(awaitable) Awaitable = decltype(*std::begin(std::declval<VecType&>()))>
    auto wait_first(VecType & awaitables) {
        POLLCORO_STATIC_ASSERT(Awaitable);

        return wait_first_iter_awaitable<VecType, Awaitable>(awaitables);
    }

    template<POLLCORO_CONCEPT(awaitable)... Awaitables>
    auto wait_first(Awaitables... awaitables) {
        POLLCORO_STATIC_ASSERT(Awaitables...);

        return wait_first_awaitable<Awaitables...>(std::move(awaitables)...);
    }

}  // namespace pollcoro