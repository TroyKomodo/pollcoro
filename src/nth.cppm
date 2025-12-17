module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <cstddef>
#include <optional>
#endif

export module pollcoro:nth;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class nth_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable& stream_;
    std::size_t n_;

    using result_type = stream_awaitable_result_t<StreamAwaitable>;
    using state_type = awaitable_state<std::optional<result_type>>;

  public:
    nth_stream_awaitable(StreamAwaitable& stream, std::size_t n) : stream_(stream), n_(n) {}

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

template<stream_awaitable StreamAwaitable>
nth_stream_awaitable(StreamAwaitable&) -> nth_stream_awaitable<StreamAwaitable>;

template<stream_awaitable StreamAwaitable>
constexpr auto nth(StreamAwaitable& stream, std::size_t n) {
    return nth_stream_awaitable<StreamAwaitable>(stream, n);
}

struct nth_stream_composable {
    std::size_t n_;
};
}  // namespace pollcoro
