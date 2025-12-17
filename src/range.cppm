module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <utility>
#endif

export module pollcoro:range;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<typename T>
class range_stream_awaitable : public awaitable_never_blocks {
    using value_type = T;
    using state_type = stream_awaitable_state<value_type>;
    T current_;
    T end_;

  public:
    range_stream_awaitable(T begin, T end) : current_(std::move(begin)), end_(std::move(end)) {}

    state_type poll_next(const waker& w) {
        if (current_ >= end_) {
            return state_type::done();
        }
        return state_type::ready(current_++);
    }
};

template<typename T>
range_stream_awaitable(T, T) -> range_stream_awaitable<T>;

template<typename T>
range_stream_awaitable(T) -> range_stream_awaitable<T>;

template<typename T>
constexpr auto range(T begin, T end) {
    return range_stream_awaitable<T>(std::move(begin), std::move(end));
}

template<typename T>
constexpr auto range(T end) {
    return range_stream_awaitable<T>(T(0), std::move(end));
}
}  // namespace pollcoro
