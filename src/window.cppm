module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#endif

export module pollcoro:window;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable, std::size_t N>
class window_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;

    using result_type = std::array<stream_awaitable_result_t<StreamAwaitable>, N>;
    using state_type = stream_awaitable_state<result_type>;

    std::array<std::optional<stream_awaitable_result_t<StreamAwaitable>>, N> buffer_;
    std::size_t index_{0};

  public:
    window_stream_awaitable(StreamAwaitable&& stream) : stream_(std::move(stream)) {}

    state_type poll_next(const waker& w) {
        while (true) {
            auto state = stream_.poll_next(w);
            if (state.is_done()) {
                return state_type::done();
            }

            if (state.is_ready()) {
                buffer_[index_++] = std::make_optional(state.take_result());
                if (index_ == N) {
                    index_ = 0;
                    result_type result;
                    for (std::size_t i = 0; i < N; i++) {
                        result[i] = std::move(*buffer_[i]);
                        buffer_[i].reset();
                    }

                    return state_type::ready(std::move(result));
                }

                continue;
            }

            return state_type::pending();
        }
    }
};

template<std::size_t N, stream_awaitable StreamAwaitable>
constexpr auto window(StreamAwaitable&& stream) {
    return window_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, N>(std::move(stream));
}

template<std::size_t N>
struct window_stream_composable {};

template<std::size_t N>
constexpr auto window() {
    return window_stream_composable<N>();
}

template<std::size_t N, stream_awaitable StreamAwaitable>
auto operator|(StreamAwaitable&& stream, window_stream_composable<N> composable) {
    return window_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, N>(std::move(stream));
}
}  // namespace pollcoro
