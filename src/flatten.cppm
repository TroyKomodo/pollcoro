module;

#include <optional>
#include <type_traits>
#include <utility>

export module pollcoro:flatten;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class flatten_stream_awaitable
    : public awaitable_maybe_blocks<StreamAwaitable, stream_awaitable_result_t<StreamAwaitable>> {
    StreamAwaitable stream_;
    std::optional<stream_awaitable_result_t<StreamAwaitable>> inner_stream_;

    using result_type = stream_awaitable_result_t<stream_awaitable_result_t<StreamAwaitable>>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    flatten_stream_awaitable(StreamAwaitable&& stream) : stream_(std::move(stream)) {}

    state_type poll_next(const waker& w) {
        while (true) {
            if (inner_stream_.has_value()) {
                auto state = inner_stream_->poll_next(w);
                if (state.is_done()) {
                    inner_stream_.reset();
                    continue;
                }

                return state;
            }

            auto state = stream_.poll_next(w);
            if (state.is_done()) {
                return state_type::done();
            }
            if (state.is_ready()) {
                inner_stream_.emplace(state.take_result());
                continue;
            }

            return state_type::pending();
        }
    }
};

template<stream_awaitable StreamAwaitable>
flatten_stream_awaitable(StreamAwaitable) -> flatten_stream_awaitable<StreamAwaitable>;

template<stream_awaitable StreamAwaitable>
constexpr auto flatten(StreamAwaitable&& stream) {
    return flatten_stream_awaitable(std::move(stream));
}

struct flatten_stream_composable {};

constexpr auto flatten() {
    return flatten_stream_composable();
}

template<stream_awaitable StreamAwaitable>
auto operator|(StreamAwaitable&& stream, flatten_stream_composable flatten) {
    return flatten_stream_awaitable(std::move(stream));
}
}  // namespace pollcoro
