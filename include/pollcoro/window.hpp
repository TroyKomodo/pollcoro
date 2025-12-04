#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

#ifndef POLLCORO_MODULE_EXPORT
#include <array>
#endif

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable, size_t N>
    class window_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
        std::decay_t<StreamAwaitable> stream_;

        using result_type = std::array<stream_awaitable_result_t<StreamAwaitable>, N>;
        using state_type = stream_awaitable_state<result_type>;

        std::array<std::optional<stream_awaitable_result_t<StreamAwaitable>>, N> buffer_;
        size_t index_{0};

      public:
        window_stream_awaitable(StreamAwaitable&& stream)
            : stream_(std::forward<StreamAwaitable>(stream)) {}

        state_type poll_next(const waker& w) {
            while (true) {
                auto state = stream_.poll_next(w);
                if (state.is_done()) {
                    return state_type::done();
                }

                if (state.is_ready()) {
                    buffer_[index_++] = std::make_optional(state.take_result());
                    if (index_ == N) {
                        index_ = 0;
                        result_type result;
                        for (size_t i = 0; i < N; i++) {
                            result[i] = std::move(*buffer_[i]);
                            buffer_[i].reset();
                        }

                        return state_type::ready(std::move(result));
                    }

                    continue;
                }

                return state_type::pending();
            }
        }
    };

    template<size_t N, POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    constexpr auto window(StreamAwaitable && stream) {
        return window_stream_awaitable<StreamAwaitable, N>(std::forward<StreamAwaitable>(stream));
    }

    template<size_t N>
    struct window_stream_composable {};

    template<size_t N>
    constexpr auto window() {
        return window_stream_composable<N>();
    }

    template<size_t N, POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    auto operator|(StreamAwaitable&& stream, window_stream_composable<N>&& composable) {
        return window_stream_awaitable<StreamAwaitable, N>(std::forward<StreamAwaitable>(stream));
    }
}  // namespace pollcoro