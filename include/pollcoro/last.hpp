#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    class last_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
        using result_type = stream_awaitable_result_t<StreamAwaitable>;
        using state_type = awaitable_state<std::optional<result_type>>;

        std::decay_t<StreamAwaitable> stream_;
        std::optional<result_type> result_;

      public:
        last_stream_awaitable(StreamAwaitable&& stream)
            : stream_(std::forward<StreamAwaitable>(stream)) {}

        state_type poll(const waker& w) {
            while (true) {
                auto state = stream_.poll_next(w);
                if (state.is_done()) {
                    return state_type::ready(std::move(result_));
                }
                if (state.is_ready()) {
                    result_ = state.take_result();
                    continue;
                }
                return state_type::pending();
            }
        }
    };

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    constexpr auto last(StreamAwaitable && stream) {
        return last_stream_awaitable<StreamAwaitable>(std::forward<StreamAwaitable>(stream));
    }
}