module;

#include <cstddef>
#include <type_traits>
#include <utility>

export module pollcoro:skip;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class skip_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    size_t count_;

    using result_type = stream_awaitable_result_t<StreamAwaitable>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    skip_stream_awaitable(StreamAwaitable stream, size_t count)
        : stream_(std::move(stream)), count_(count) {}

    state_type poll_next(const waker& w) {
        while (true) {
            if (count_ == 0) {
                return stream_.poll_next(w);
            }

            auto state = stream_.poll_next(w);
            if (state.is_done()) {
                return state_type::done();
            }

            if (state.is_ready()) {
                count_--;
                continue;
            }

            return state_type::pending();
        }
    }
};

template<stream_awaitable StreamAwaitable>
constexpr auto skip(StreamAwaitable stream, size_t count) {
    return skip_stream_awaitable<StreamAwaitable>(std::move(stream), count);
}

struct skip_stream_composable {
    size_t count_;
};

template<stream_awaitable StreamAwaitable>
auto operator|(StreamAwaitable stream, skip_stream_composable composable) {
    return skip_stream_awaitable<StreamAwaitable>(std::move(stream), composable.count_);
}

constexpr auto skip(size_t count) {
    return skip_stream_composable(count);
}
}  // namespace pollcoro
