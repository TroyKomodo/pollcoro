#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    class nth_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
        StreamAwaitable& stream_;
        size_t n_;

        using result_type = stream_awaitable_result_t<StreamAwaitable>;
        using state_type = awaitable_state<std::optional<result_type>>;

      public:
        nth_stream_awaitable(StreamAwaitable& stream, size_t n) : stream_(stream), n_(n) {}

        state_type poll(const waker& w) {
            while (true) {
                auto state = stream_.poll_next(w);
                if (state.is_done()) {
                    return state_type::ready(std::nullopt);
                }
                if (!state.is_ready()) {
                    return state_type::pending();
                }
                n_--;
                if (n_ == 0) {
                    return state_type::ready(std::make_optional(state.take_result()));
                }
            }
        }
    };

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    constexpr auto nth(StreamAwaitable & stream, size_t n) {
        return nth_stream_awaitable<StreamAwaitable>(stream, n);
    }

    struct nth_stream_composable {
        size_t n_;
    };
}