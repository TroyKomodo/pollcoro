module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <optional>
#include <type_traits>
#include <utility>
#endif

export module pollcoro:last;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class last_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    using result_type = stream_awaitable_result_t<StreamAwaitable>;
    using state_type = awaitable_state<std::optional<result_type>>;

    StreamAwaitable stream_;
    std::optional<result_type> result_;

  public:
    last_stream_awaitable(StreamAwaitable&& stream) : stream_(std::move(stream)) {}

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

template<stream_awaitable StreamAwaitable>
last_stream_awaitable(StreamAwaitable) -> last_stream_awaitable<StreamAwaitable>;

template<stream_awaitable StreamAwaitable>
constexpr auto last(StreamAwaitable&& stream) {
    return last_stream_awaitable<std::remove_cvref_t<StreamAwaitable>>(std::move(stream));
}
}  // namespace pollcoro
