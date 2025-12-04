#pragma once

#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

#ifndef POLLCORO_MODULE_EXPORT
#include <functional>
#endif

POLLCORO_EXPORT namespace pollcoro {
    namespace detail {

#if POLLCORO_USE_CONCEPTS
    // Make sure predicate takes const reference
    template<typename Predicate, typename T>
    concept skip_while_predicate = requires(Predicate predicate, const T& result) {
        { predicate(result) } -> std::same_as<bool>;
    };
#endif

    template<typename Predicate, typename T>
    inline constexpr bool is_skip_while_predicate_v = std::is_invocable_v<Predicate, const T&>;
    }  // namespace detail

    template<
        POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable,
        POLLCORO_CONCEPT(detail::skip_while_predicate<stream_awaitable_result_t<StreamAwaitable>>)
            Predicate>
    class skip_while_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
        std::decay_t<StreamAwaitable> stream_;
        std::decay_t<Predicate> predicate_;
        bool skipping_{true};

        using result_type = stream_awaitable_result_t<StreamAwaitable>;
        using state_type = stream_awaitable_state<result_type>;

        static_assert(
            detail::is_skip_while_predicate_v<Predicate, result_type>,
            "Predicate must take const reference to result type"
        );

      public:
        skip_while_stream_awaitable(StreamAwaitable&& stream, Predicate&& predicate)
            : stream_(std::forward<StreamAwaitable>(stream)),
              predicate_(std::forward<Predicate>(predicate)) {}

        state_type poll_next(const waker& w) {
            while (true) {
                auto state = stream_.poll_next(w);
                if (state.is_done()) {
                    return state_type::done();
                }
                if (state.is_ready() && skipping_) {
                    auto result = state.take_result();
                    skipping_ = std::invoke(predicate_, result);
                    if (skipping_) {
                        continue;
                    }
                    return state_type::ready(std::move(result));
                }
                return state;
            }
        }
    };

    template<
        POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable,
        POLLCORO_CONCEPT(detail::skip_while_predicate<stream_awaitable_result_t<StreamAwaitable>>)
            Predicate>
    constexpr auto skip_while(StreamAwaitable && stream, Predicate && predicate) {
        return skip_while_stream_awaitable<StreamAwaitable, Predicate>(
            std::forward<StreamAwaitable>(stream), std::forward<Predicate>(predicate)
        );
    }

    template<typename Predicate>
    struct skip_while_stream_composable {
        std::decay_t<Predicate> predicate_;
    };

    template<
        POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable,
        POLLCORO_CONCEPT(detail::skip_while_predicate<stream_awaitable_result_t<StreamAwaitable>>)
            Predicate>
    auto operator|(StreamAwaitable&& stream, skip_while_stream_composable<Predicate>&& composable) {
        return skip_while_stream_awaitable<StreamAwaitable, Predicate>(
            std::forward<StreamAwaitable>(stream), std::forward<Predicate>(composable.predicate_)
        );
    }

    template<typename Predicate>
    constexpr auto skip_while(Predicate && predicate) {
        return skip_while_stream_composable<Predicate>(std::forward<Predicate>(predicate));
    }
}  // namespace pollcoro