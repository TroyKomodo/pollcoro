module;

#include <cstddef>
#include <type_traits>
#include <utility>

export module pollcoro:enumerate;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class enumerate_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    size_t index_{0};

    using result_type = std::pair<size_t, stream_awaitable_result_t<StreamAwaitable>>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    enumerate_stream_awaitable(StreamAwaitable stream) : stream_(std::move(stream)) {}

    state_type poll_next(const waker& w) {
        auto state = stream_.poll_next(w);
        if (state.is_done()) {
            return state_type::done();
        }
        if (state.is_ready()) {
            return state_type::ready(std::make_pair(index_++, state.take_result()));
        }
        return state_type::pending();
    }
};

template<stream_awaitable StreamAwaitable>
enumerate_stream_awaitable(StreamAwaitable) -> enumerate_stream_awaitable<StreamAwaitable>;

template<stream_awaitable StreamAwaitable>
constexpr auto enumerate(StreamAwaitable stream) {
    return enumerate_stream_awaitable<StreamAwaitable>(std::move(stream));
}

class enumerate_stream_composable : public awaitable_never_blocks {
    size_t index_{0};

    using state_type = stream_awaitable_state<size_t>;

  public:
    state_type poll_next(const waker& w) {
        return state_type::ready(index_++);
    }
};

constexpr auto enumerate() {
    return enumerate_stream_composable();
}

template<stream_awaitable StreamAwaitable>
auto operator|(StreamAwaitable stream, enumerate_stream_composable enumerate) {
    return enumerate_stream_awaitable<StreamAwaitable>(std::move(stream));
}
}  // namespace pollcoro
