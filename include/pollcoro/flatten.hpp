#pragma once

#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    class flatten_stream_awaitable : public awaitable_maybe_blocks<
                                         StreamAwaitable,
                                         stream_awaitable_result_t<StreamAwaitable>> {
        std::decay_t<StreamAwaitable> stream_;
        std::optional<stream_awaitable_result_t<StreamAwaitable>> inner_stream_;

        using result_type = stream_awaitable_result_t<stream_awaitable_result_t<StreamAwaitable>>;
        using state_type = stream_awaitable_state<result_type>;

      public:
        flatten_stream_awaitable(StreamAwaitable&& stream)
            : stream_(std::forward<StreamAwaitable>(stream)) {}

        state_type poll_next(const waker& w) {
            while (true) {
                if (inner_stream_.has_value()) {
                    auto state = inner_stream_->poll_next(w);
                    if (state.is_done()) {
                        inner_stream_ = std::nullopt;
                        continue;
                    }

                    return state;
                }

                auto state = stream_.poll_next(w);
                if (state.is_done()) {
                    return state_type::done();
                }
                if (state.is_ready()) {
                    inner_stream_ = std::make_optional(state.take_result());
                    continue;
                }

                return state_type::pending();
            }
        }
    };

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    constexpr auto flatten(StreamAwaitable && stream) {
        return flatten_stream_awaitable<StreamAwaitable>(std::forward<StreamAwaitable>(stream));
    }

    struct flatten_stream_composable {};

    constexpr auto flatten() {
        return flatten_stream_composable();
    }

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    auto operator|(StreamAwaitable&& stream, flatten_stream_composable&& flatten) {
        return flatten_stream_awaitable<StreamAwaitable>(std::forward<StreamAwaitable>(stream));
    }
}  // namespace pollcoro