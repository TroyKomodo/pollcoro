module;

#include <optional>

export module pollcoro:next;

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class stream_next_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable& stream_;

    using result_type = std::optional<stream_awaitable_result_t<StreamAwaitable>>;
    using state_type = awaitable_state<result_type>;

  public:
    stream_next_awaitable(StreamAwaitable& stream) : stream_(stream) {}

    state_type poll(const waker& w) {
        auto state = stream_.poll_next(w);
        if (state.is_done()) {
            return state_type::ready(std::nullopt);
        }
        if (state.is_ready()) {
            return state_type::ready(std::make_optional(state.take_result()));
        }
        return state_type::pending();
    }
};

template<stream_awaitable StreamAwaitable>
constexpr auto next(StreamAwaitable& stream) {
    return stream_next_awaitable<StreamAwaitable>(stream);
}

}  // namespace pollcoro
