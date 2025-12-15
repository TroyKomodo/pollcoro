module;

#include <cstddef>
#include <type_traits>
#include <utility>

export module pollcoro:take;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class take_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    size_t count_;

    using result_type = stream_awaitable_result_t<StreamAwaitable>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    take_stream_awaitable(StreamAwaitable&& stream, size_t count)
        : stream_(std::move(stream)), count_(count) {}

    state_type poll_next(const waker& w) {
        if (count_ == 0) {
            return state_type::done();
        }
        auto state = stream_.poll_next(w);
        if (state.is_ready()) {
            count_--;
        }
        return state;
    }
};

template<stream_awaitable StreamAwaitable>
constexpr auto take(StreamAwaitable&& stream, size_t count) {
    return take_stream_awaitable<std::remove_cvref_t<StreamAwaitable>>(std::move(stream), count);
}

struct take_stream_composable {
    size_t count_;
};

template<stream_awaitable StreamAwaitable>
auto operator|(StreamAwaitable&& stream, take_stream_composable composable) {
    return take_stream_awaitable<std::remove_cvref_t<StreamAwaitable>>(
        std::move(stream), composable.count_
    );
}

constexpr auto take(size_t count) {
    return take_stream_composable(count);
}
}  // namespace pollcoro
